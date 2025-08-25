#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "g_config.h"
#include "rgb_capture.h"
#include "pio_programs.h"
#include "v_buf.h"

static int dma_ch0;
static int dma_ch1;
const pio_program_t *program = NULL;
static uint16_t offset;

uint8_t *cap_buf;
settings_t capture_settings;

uint32_t frame_count = 0;

uint h_sync_pulse_2;
uint v_sync_pulse;

static uint32_t cap_dma_buf[2][CAP_DMA_BUF_SIZE / 4];
static uint32_t *cap_dma_buf_addr[2];

void check_settings(settings_t *settings)
{
  if (settings->video_out_mode > VIDEO_OUT_MODE_MAX ||
      settings->video_out_mode < VIDEO_OUT_MODE_MIN)
    settings->video_out_mode = VIDEO_OUT_MODE_DEF;

  if (settings->cap_sync_mode > CAP_SYNC_MODE_MAX ||
      settings->cap_sync_mode < CAP_SYNC_MODE_MIN)
    settings->cap_sync_mode = CAP_SYNC_MODE_DEF;

  if (settings->frequency > FREQUENCY_MAX ||
      settings->frequency < FREQUENCY_MIN)
    settings->frequency = FREQUENCY_DEF;

  if (settings->ext_clk_divider > EXT_CLK_DIVIDER_MAX ||
      settings->ext_clk_divider < EXT_CLK_DIVIDER_MIN)
    settings->ext_clk_divider = EXT_CLK_DIVIDER_DEF;

  if (settings->delay > DELAY_MAX ||
      settings->delay < DELAY_MIN)
    settings->delay = DELAY_DEF;

  if (settings->shX > shX_MAX ||
      settings->shX < shX_MIN)
    settings->shX = shX_DEF;

  if (settings->shY > shY_MAX ||
      settings->shY < shY_MIN)
    settings->shY = shY_DEF;

  if (settings->pin_inversion_mask & ~PIN_INVERSION_MASK)
  {
    settings->pin_inversion_mask = PIN_INVERSION_MASK_DEF;
    settings->scanlines_mode = false;
    settings->x3_buffering_mode = false;
    settings->video_sync_mode = false;
  }
}

void set_capture_settings(settings_t *settings)
{
  memcpy(&capture_settings, settings, sizeof(settings_t));
  check_settings(&capture_settings);
}

void set_capture_frequency(uint32_t frequency)
{
  uint16_t div_int;
  uint8_t div_frac;

  if (capture_settings.cap_sync_mode == SELF)
  {
    capture_settings.frequency = frequency;

    pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (capture_settings.frequency * 12.0), &div_int, &div_frac);

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
    capture_settings.ext_clk_divider = EXT_CLK_DIVIDER_MAX;
  else if (divider < EXT_CLK_DIVIDER_MIN)
    capture_settings.ext_clk_divider = EXT_CLK_DIVIDER_MIN;
  else
    capture_settings.ext_clk_divider = divider;

  if (capture_settings.cap_sync_mode == EXT)
  {
    PIO_CAP->instr_mem[offset + pio_capture_1_offset_divider1] = set_opcode | (capture_settings.ext_clk_divider - 1);
    PIO_CAP->instr_mem[offset + pio_capture_1_offset_divider2] = set_opcode | (capture_settings.ext_clk_divider - 1);
  }

  return capture_settings.ext_clk_divider;
}

int16_t set_capture_shX(int16_t shX)
{
  if (shX > shX_MAX)
    capture_settings.shX = shX_MAX;
  else if (shX < shX_MIN)
    capture_settings.shX = shX_MIN;
  else
    capture_settings.shX = shX;

  return capture_settings.shX;
}

int16_t set_capture_shY(int16_t shY)
{
  if (shY > shY_MAX)
    capture_settings.shY = shY_MAX;
  else if (shY < shY_MIN)
    capture_settings.shY = shY_MIN;
  else
    capture_settings.shY = shY;

  return capture_settings.shY;
}

int8_t set_capture_delay(int8_t delay)
{
  if (delay > DELAY_MAX)
    capture_settings.delay = DELAY_MAX;
  else if (delay < DELAY_MIN)
    capture_settings.delay = DELAY_MIN;
  else
    capture_settings.delay = delay;

  uint16_t pio_capture_offset_delay = capture_settings.cap_sync_mode == SELF ? pio_capture_0_offset_delay : pio_capture_1_offset_delay;
  PIO_CAP->instr_mem[offset + pio_capture_offset_delay] = nop_opcode | (capture_settings.delay << 8);

  return capture_settings.delay;
}

void set_pin_inversion_mask(uint8_t pin_inversion_mask)
{
  capture_settings.pin_inversion_mask = pin_inversion_mask;

  for (int i = CAP_PIN_D0; i < CAP_PIN_D0 + 7; i++)
  {
    gpio_set_inover(i, pin_inversion_mask & 1);
    pin_inversion_mask >>= 1;
  }
}

void set_video_sync_mode(bool video_sync_mode)
{
  capture_settings.video_sync_mode = video_sync_mode;
}

void __not_in_flash_func(dma_handler_capture())
{
  static uint32_t dma_buf_idx;
  static uint8_t pix8_s;
  static int x_s;
  static int y_s;

  uint8_t sync_mask = capture_settings.video_sync_mode ? ((1u << VS_PIN) | (1u << HS_PIN)) : (1u << HS_PIN);

  uint hsync_pulse_2 = h_sync_pulse_2;
  uint vsync_pulse = v_sync_pulse;

  dma_hw->ints1 = 1u << dma_ch1;
  dma_channel_set_read_addr(dma_ch1, &cap_dma_buf_addr[dma_buf_idx & 1], false);

  int shX = capture_settings.shX;
  int shY = capture_settings.shY;

  uint8_t *buf8 = (uint8_t *)cap_dma_buf[dma_buf_idx & 1];
  dma_buf_idx++;

  register uint8_t pix8 = pix8_s;
  register int x = x_s;
  register int y = y_s;

  static uint8_t *cap_buf8_s = g_v_buf;
  uint8_t *cap_buf8 = cap_buf8_s;

  static uint CS_idx_s = 0;
  uint CS_idx = CS_idx_s;

  for (int k = CAP_DMA_BUF_SIZE; k--;)
  {
    uint8_t val8 = *buf8++;

    x++;

    if ((val8 & sync_mask) != sync_mask) // detect active sync pulses
    {
      if (CS_idx == hsync_pulse_2) // start in the middle of the H_SYNC pulse // this should help ignore the spikes
      {
        y++;
        // set the pointer to the beginning of a new line
        if ((y >= 0) && (cap_buf != NULL))
          cap_buf8 = &(((uint8_t *)cap_buf)[y * V_BUF_W / 2]);
      }

      CS_idx++;
      x = -shX - 1;

      if (sync_mask == (1u << HS_PIN)) // composite sync
      {
        if (CS_idx < vsync_pulse) // detect V_SYNC pulse
          continue;
      }
      else if (val8 & (1u << VS_PIN))
        continue;

      // start capture of a new frame
      if (y >= 0)
      {
        if (frame_count > 10) // power on delay // noise immunity at the sync input
          cap_buf = get_v_buf_in();

        frame_count++;
      }

      y = -shY - 1;
      continue;
    }

    if (x & 1)
    {
      if (cap_buf == NULL)
        continue;

      if (x < 0)
        continue;

      if (y < 0)
        continue;

      if (x >= V_BUF_W)
        continue;

      if (y >= V_BUF_H)
        continue;

      *cap_buf8++ = (pix8 & 0xf) | (val8 << 4);
    }
    else
    {
      CS_idx = 0;
      pix8 = val8;
    }
  }

  x_s = x;
  y_s = y;
  pix8_s = pix8;
  cap_buf8_s = cap_buf8;
  CS_idx_s = CS_idx;
}

void start_capture(settings_t *settings)
{
  set_capture_settings(settings);

  uint8_t pin_inversion_mask = capture_settings.pin_inversion_mask;

  // video timing variables measured in pixels
  h_sync_pulse_2 = 3 * (capture_settings.frequency / 1000000); // 3 µs - 1/2 of the H_SYNC pulse
  v_sync_pulse = 30 * (capture_settings.frequency / 1000000);  // 30 µs - V_SYNC pulse

  // set capture pins
  for (int i = CAP_PIN_D0; i < CAP_PIN_D0 + 7; i++)
  {
    gpio_init(i);
    gpio_set_dir(i, GPIO_IN);
    gpio_set_input_hysteresis_enabled(i, true);

    gpio_set_inover(i, pin_inversion_mask & 1);

    pin_inversion_mask >>= 1;
  }

  // PIO initialization
  switch (capture_settings.cap_sync_mode)
  {
  case SELF:
  {
    program = &pio_capture_0_program;
    break;
  }

  case EXT:
  {
    program = &pio_capture_1_program;
    break;
  }

  default:
    break;
  }

  // load PIO program
  offset = pio_add_program(PIO_CAP, program);
  uint wrap = offset + program->length - 1;

  // set initial capture delay
  set_capture_delay(capture_settings.delay);

  // set external clock divider
  set_ext_clk_divider(capture_settings.ext_clk_divider);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset, wrap);

  sm_config_set_in_shift(&c, false, false, 8); // autopush not needed
  sm_config_set_in_pins(&c, CAP_PIN_D0);
  sm_config_set_jmp_pin(&c, HS_PIN);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  if (capture_settings.cap_sync_mode == SELF)
  {
    uint16_t div_int;
    uint8_t div_frac;

    pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (capture_settings.frequency * 12.0), &div_int, &div_frac);
    sm_config_set_clkdiv_int_frac(&c, div_int, div_frac);
  }

  pio_sm_init(PIO_CAP, SM_CAP, offset, &c);
  pio_sm_set_enabled(PIO_CAP, SM_CAP, true);

  // DMA initialization
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);

  // main (data) DMA channel
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);

  channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);
  channel_config_set_read_increment(&c0, false);
  channel_config_set_write_increment(&c0, true);
  channel_config_set_dreq(&c0, DREQ_PIO_CAP + SM_CAP);
  channel_config_set_chain_to(&c0, dma_ch1);

  dma_channel_configure(
      dma_ch0,
      &c0,
      &cap_dma_buf[0][0],    // write address
      &PIO_CAP->rxf[SM_CAP], // read address
      CAP_DMA_BUF_SIZE,      //
      false                  // don't start yet
  );

  // control DMA channel
  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);

  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_chain_to(&c1, dma_ch0); // chain to other channel

  cap_dma_buf_addr[0] = &cap_dma_buf[0][0];
  cap_dma_buf_addr[1] = &cap_dma_buf[1][0];

  dma_channel_configure(
      dma_ch1,
      &c1,
      &dma_hw->ch[dma_ch0].write_addr, // write address
      &cap_dma_buf_addr[0],            // read address
      1,                               //
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
  pio_sm_set_enabled(PIO_CAP, SM_CAP, false);
  pio_sm_init(PIO_CAP, SM_CAP, offset, NULL);
  pio_remove_program(PIO_CAP, program, offset);
  dma_channel_set_irq1_enabled(dma_ch1, false);
  dma_channel_abort(dma_ch0);
  dma_channel_abort(dma_ch1);
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);
}
