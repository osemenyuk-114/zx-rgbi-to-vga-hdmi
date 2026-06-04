#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "g_config.h"
#include "rgb_capture.h"
#include "pio_programs.h"
#include "v_buf.h"

// Ring buffer configuration
#define CAP_LINE_LENGTH 1024
#define CAP_DMA_BUF_COUNT 16     // 16 line buffers for better granularity
#define CAP_DMA_BUF_COUNT_LOG2 4 // log2(16) for ring wrapping

extern settings_t settings;

static int dma_ch0;
static int dma_ch1;
static uint offset;
static const pio_program_t *program = NULL;

static uint16_t h_sync_pulse_2;
static uint16_t v_sync_pulse;
static volatile uint8_t capture_sync_mask = (uint8_t)(1u << HS_PIN);

volatile uint32_t frame_count = 0;

// Ring buffer: 16 line buffers
static uint8_t cap_dma_buf[CAP_DMA_BUF_COUNT][CAP_LINE_LENGTH];
static uint8_t *cap_dma_buf_addr[CAP_DMA_BUF_COUNT] __attribute__((aligned(CAP_DMA_BUF_COUNT * 4)));

// DMA handler persistent state (file-scope for reset_capture_state access)
static int cap_x_s;
static int cap_y_s;
static uint cap_CS_idx_s;
static uint8_t cap_pix8_s;
static uint8_t *cap_buf8_s;
static uint8_t *cap_buf;
static uint32_t cap_active_buf_idx;

void set_capture_frequency(uint32_t frequency)
{
  uint16_t div_int;
  uint8_t div_frac;

  if (settings.cap_sync_mode == SELF)
  {
    settings.frequency = frequency;

    pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (settings.frequency * 12.0), &div_int, &div_frac);

    static_assert(REG_FIELD_WIDTH(PIO_SM0_CLKDIV_INT) == 16, "");
    invalid_params_if(HARDWARE_PIO, div_int >> 16);
    invalid_params_if(HARDWARE_PIO, div_int == 0 && div_frac != 0);
    static_assert(REG_FIELD_WIDTH(PIO_SM0_CLKDIV_FRAC) == 8, "");

    PIO_CAP->sm[SM_CAP].clkdiv = (((uint)div_frac) << PIO_SM0_CLKDIV_FRAC_LSB) | (((uint)div_int) << PIO_SM0_CLKDIV_INT_LSB);

    pio_sm_clkdiv_restart(PIO_CAP, SM_CAP);
  }
}

int8_t set_ext_clk_divider(int8_t divider)
{
  if (divider > EXT_CLK_DIVIDER_MAX)
    settings.ext_clk_divider = EXT_CLK_DIVIDER_MAX;
  else if (divider < EXT_CLK_DIVIDER_MIN)
    settings.ext_clk_divider = EXT_CLK_DIVIDER_MIN;
  else
    settings.ext_clk_divider = divider;

  if (settings.cap_sync_mode == EXT)
  {
    PIO_CAP->instr_mem[offset + pio_capture_1_offset_divider1] = set_opcode | (settings.ext_clk_divider - 1);
    PIO_CAP->instr_mem[offset + pio_capture_1_offset_divider2] = set_opcode | (settings.ext_clk_divider - 1);
  }

  return settings.ext_clk_divider;
}

int16_t set_capture_shX(int16_t shX)
{
  if (shX > shX_MAX)
    settings.shX = shX_MAX;
  else if (shX < shX_MIN)
    settings.shX = shX_MIN;
  else
    settings.shX = shX;

  return settings.shX;
}

int16_t set_capture_shY(int16_t shY)
{
  if (shY > shY_MAX)
    settings.shY = shY_MAX;
  else if (shY < shY_MIN)
    settings.shY = shY_MIN;
  else
    settings.shY = shY;

  return settings.shY;
}

int8_t set_capture_delay(int8_t delay)
{
  if (delay > DELAY_MAX)
    settings.delay = DELAY_MAX;
  else if (delay < DELAY_MIN)
    settings.delay = DELAY_MIN;
  else
    settings.delay = delay;

  uint16_t pio_capture_offset_delay = settings.cap_sync_mode == SELF ? pio_capture_0_offset_delay : pio_capture_1_offset_delay;
  PIO_CAP->instr_mem[offset + pio_capture_offset_delay] = nop_opcode | (settings.delay << 8);

  return settings.delay;
}

void set_pin_inversion_mask(uint8_t pin_inversion_mask)
{
  settings.pin_inversion_mask = pin_inversion_mask;

  for (int i = CAP_PIN_D0; i < CAP_PIN_D0 + 7; i++)
  {
    gpio_set_inover(i, pin_inversion_mask & 1);
    pin_inversion_mask >>= 1;
  }
}

static inline void update_capture_sync_mask(bool video_sync_mode)
{
  capture_sync_mask = video_sync_mode ? (uint8_t)((1u << HS_PIN) | (1u << VS_PIN)) : (uint8_t)(1u << HS_PIN);
}

void set_video_sync_mode(bool video_sync_mode)
{
  settings.video_sync_mode = video_sync_mode;
  update_capture_sync_mask(video_sync_mode);
}

void __attribute__((hot)) __not_in_flash_func(dma_handler_capture())
{
  int x = cap_x_s;
  int y = cap_y_s;
  uint CS_idx = cap_CS_idx_s;
  uint8_t pix8 = cap_pix8_s;

  const int shX = settings.shX;
  const int shY = settings.shY;
  const bool video_sync_mode = settings.video_sync_mode;
  const uint8_t sync_mask = capture_sync_mask;

  uint8_t *cap_buf8 = cap_buf8_s;

  dma_hw->ints1 = 1u << dma_ch1;

  uint32_t cur_buf_idx = cap_active_buf_idx % CAP_DMA_BUF_COUNT;

  uint8_t *buf8 = cap_dma_buf[cur_buf_idx];
  uint8_t *const buf8_end = buf8 + CAP_LINE_LENGTH;

  cap_active_buf_idx++;

  while (buf8 < buf8_end)
  {
    uint8_t val8 = *buf8++;

    x++;

    // Active video is the common path; handle it first and continue.
    if ((val8 & sync_mask) == sync_mask)
    {
      // Even sample: cache low nibble source and reset sync pulse counter.
      if ((x & 1) == 0)
      {
        CS_idx = 0;
        pix8 = val8;
        continue;
      }

      // Odd sample: pack two 4-bit pixels into one byte.
      if (cap_buf && (unsigned)x < V_BUF_W && (unsigned)y < V_BUF_H)
        *cap_buf8++ = (uint8_t)((pix8 & 0x0f) | (val8 << 4));

      continue;
    }

    // Detect active sync pulses.
    if (CS_idx == h_sync_pulse_2)
    {
      y++;

      // Set the pointer to the beginning of a new line.
      if ((y >= 0) && cap_buf)
        cap_buf8 = &cap_buf[y * (V_BUF_W / 2)];
    }

    CS_idx++;
    x = -shX - 1;

    if (!video_sync_mode)
    {
      // Composite sync: detect V_SYNC pulse by pulse width.
      if (CS_idx < v_sync_pulse)
        continue;
    }
    else if (val8 & (1u << VS_PIN))
      continue;

    if (y >= 0)
    {
      // Start capture of a new frame (with startup noise immunity).
      if (frame_count > 10)
        cap_buf = get_v_buf_in();
      else if (frame_count == 5)
        clear_video_buffers();

      frame_count++;
    }

    y = -shY - 1;
  }

  cap_x_s = x;
  cap_y_s = y;
  cap_pix8_s = pix8;
  cap_buf8_s = cap_buf8;
  cap_CS_idx_s = CS_idx;
}

void start_capture()
{
  // Reset capture handler state (video buffers cleared later at frame_count == 5)
  cap_x_s = 0;
  cap_y_s = 0;
  cap_CS_idx_s = 0;
  cap_pix8_s = 0;
  cap_buf8_s = g_v_buf;
  cap_buf = NULL;
  cap_active_buf_idx = 0;
  frame_count = 0;

  uint8_t pin_inversion_mask = settings.pin_inversion_mask;

  update_capture_sync_mask(settings.video_sync_mode);

  // video timing variables measured in pixels
  h_sync_pulse_2 = 3 * settings.frequency / 1000000; // 3 µs - 1/2 of the H_SYNC pulse
  v_sync_pulse = 30 * settings.frequency / 1000000;  // 30 µs - V_SYNC pulse

  // set capture pins
  for (int i = CAP_PIN_D0; i < CAP_PIN_D0 + 7; i++)
  {
    pio_gpio_init(PIO_CAP, i);
    gpio_set_input_hysteresis_enabled(i, true);
    gpio_set_inover(i, pin_inversion_mask & 1);

    pin_inversion_mask >>= 1;
  }

  // Initialize ring buffer address array
  for (int i = 0; i < CAP_DMA_BUF_COUNT; i++)
    cap_dma_buf_addr[i] = cap_dma_buf[i];

  // PIO initialization
  pio_sm_config c = pio_get_default_sm_config();

  switch (settings.cap_sync_mode)
  {
  case SELF:
    program = &pio_capture_0_program;
    break;

  case EXT:
    program = &pio_capture_1_program;
    break;

  default:
    break;
  }

  // load PIO program
  offset = pio_add_program(PIO_CAP, program);
  sm_config_set_wrap(&c, offset, offset + program->length - 1);

  // set capture parameters
  set_capture_delay(settings.delay);
  set_ext_clk_divider(settings.ext_clk_divider);

  sm_config_set_in_pins(&c, CAP_PIN_D0);
  sm_config_set_jmp_pin(&c, HS_PIN);

  sm_config_set_in_shift(&c, true, false, 32); // 32-bit push with direct byte-order DMA reads
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  uint16_t div_int = 1;
  uint8_t div_frac = 0;

  if (settings.cap_sync_mode == SELF)
    pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (settings.frequency * 12.0), &div_int, &div_frac);

  sm_config_set_clkdiv_int_frac(&c, div_int, div_frac);

  pio_sm_init(PIO_CAP, SM_CAP, offset, &c);
  pio_sm_set_enabled(PIO_CAP, SM_CAP, true);

  // DMA initialization
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);

  // main (data) DMA channel
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);

  channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
  channel_config_set_read_increment(&c0, false);
  channel_config_set_write_increment(&c0, true);
  channel_config_set_dreq(&c0, DREQ_PIO_CAP + SM_CAP);
  channel_config_set_chain_to(&c0, dma_ch1);

  dma_channel_configure(
      dma_ch0,
      &c0,
      cap_dma_buf[0],        // write address (will be updated by control channel)
      &PIO_CAP->rxf[SM_CAP], // read address
      CAP_LINE_LENGTH / 4,   // transfer count in 32-bit words (1024 bytes / 4)
      false                  // don't start yet
  );

  // control DMA channel
  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);

  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, true);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_ring(&c1, false, 2 + CAP_DMA_BUF_COUNT_LOG2); // ring on read address
  channel_config_set_chain_to(&c1, dma_ch0);                       // chain to data channel

  dma_channel_configure(
      dma_ch1,
      &c1,
      &dma_hw->ch[dma_ch0].write_addr, // write address
      cap_dma_buf_addr,                // read address (with ring wrapping)
      1,                               // transfer 1 address pointer
      false                            // don't start yet
  );

  dma_channel_set_irq1_enabled(dma_ch1, true);

  // configure the processor to run dma_handler() when DMA IRQ 0 is asserted
  irq_set_exclusive_handler(DMA_IRQ_1, dma_handler_capture);
  irq_set_enabled(DMA_IRQ_1, true);

  dma_start_channel_mask((1u << dma_ch0));
}

void stop_capture()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_1, false);

  // clear the IRQ handler to prevent conflicts with restarting capture
  irq_remove_handler(DMA_IRQ_1, dma_handler_capture);

  // stop PIO
  pio_sm_set_enabled(PIO_CAP, SM_CAP, false);
  pio_sm_init(PIO_CAP, SM_CAP, offset, NULL);
  pio_remove_program(PIO_CAP, program, offset);

  // cleanup and free DMA channels
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);
}