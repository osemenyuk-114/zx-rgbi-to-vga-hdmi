#include "g_config.h"
#include "vga.h"
#include "pio_programs.h"
#include "v_buf.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"
#include "hardware/vreg.h"

static int dma_ch1;
static video_mode_t video_mode;

static int16_t h_visible_area;
static int16_t v_visible_area;
static int16_t v_margin;
static bool scanlines_mode = false;

static uint32_t *line_patterns[4];
static uint8_t palette[16];

#ifdef BOARD_CODE_36LJU22
uint8_t palette8[] = {
    0b00000000,
    0b00100000,
    0b00001000,
    0b00101000,
    0b00000010,
    0b00100010,
    0b00001010,
    0b00101010,

    0b00000000,
    0b00110000,
    0b00001100,
    0b00111100,
    0b00000011,
    0b00110011,
    0b00001111,
    0b00111111,
};
#else
uint8_t palette8[] = {
    0b00000000,
    0b00100000,
    0b00001000,
    0b00101000,
    0b00000010,
    0b00100010,
    0b00001010,
    0b00101010,

    0b00000000,
    0b00110000,
    0b00001100,
    0b00111100,
    0b00000011,
    0b00110011,
    0b00001111,
    0b00111111,
};
#endif

void __not_in_flash_func(memset32)(uint32_t *dst, const uint32_t data, uint32_t size);

void __not_in_flash_func(dma_handler_vga)()
{
  dma_hw->ints0 = 1u << dma_ch1;
  static uint16_t y = 0;
  static uint8_t *screen_buf = NULL;

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    screen_buf = get_v_buf_out();
  }

  if (y >= video_mode.v_visible_area && y < (video_mode.v_visible_area + video_mode.v_front_porch))
  {
    // VS front
    dma_channel_set_read_addr(dma_ch1, &line_patterns[0], false);
    return;
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  {
    // VS SYNC
    dma_channel_set_read_addr(dma_ch1, &line_patterns[1], false);
    return;
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse) && y < video_mode.whole_frame)
  {
    dma_channel_set_read_addr(dma_ch1, &line_patterns[0], false);
    return;
  }

  if (!(screen_buf))
  {
    dma_channel_set_read_addr(dma_ch1, &line_patterns[2], false);
    return;
  }

  if (y < v_margin || y > (v_visible_area + v_margin))
  {
    dma_channel_set_read_addr(dma_ch1, &line_patterns[0], false);
    return;
  }

  // image area
  uint32_t **v_out_dma_buf_addr = &line_patterns[2];
  uint8_t *scr_buf = &screen_buf[(uint16_t)((y - v_margin) / video_mode.div) * V_BUF_W / 2];

  uint8_t line = y % (2 * video_mode.div);

  // correct line mapping for different
  switch (video_mode.div)
  {
  case 2:
  {
    /* scanlines mode not enabled due to reduced image brightness and uneven line thickness caused by monitor scaler
    if (scanlines_mode)
      {
        if (line > 0)
          line++;

        if (line == 4)
          line++;
      }
      else
    */
    if (line > 1)
      line++;

    break;
  }

  case 3:
  {
    if (!scanlines_mode && ((line == 2) || (line == 5)))
      line--;

    break;
  }

  case 4:
  {
    if (scanlines_mode)
    {
      if (1) // (1) - scanline on 1 of 4 lines, (0) - 1 of 2
      {
        if (line > 1)
          line--;

        if (line == 5)
          line--;
      }
      else if (line > 2)
        line--;

      if (line == 6)
        line--;
    }
    else
    {
      if (line > 2)
        line--;

      if (line == 6)
        line--;

      if ((line == 2) || (line == 5))
        line--;
    }

    break;
  }

  default:
    break;
  }

  switch (line)
  {
  case 0:
    v_out_dma_buf_addr = &line_patterns[2];
    break;

  case 1:
    dma_channel_set_read_addr(dma_ch1, &line_patterns[2], false);
    return;

  case 2:
    dma_channel_set_read_addr(dma_ch1, &line_patterns[0], false);
    return;

  case 3:
    v_out_dma_buf_addr = &line_patterns[3];
    break;

  case 4:
    dma_channel_set_read_addr(dma_ch1, &line_patterns[3], false);
    return;

  case 5:
    dma_channel_set_read_addr(dma_ch1, &line_patterns[0], false);
    return;

  default:
    break;
  }

  uint8_t *line_buf = (uint8_t *)(*v_out_dma_buf_addr);

  for (int i = h_visible_area; i--;)
  {
    *line_buf++ = palette[*scr_buf & 0xf];
    *line_buf++ = palette[*scr_buf++ >> 4];
  }

  dma_channel_set_read_addr(dma_ch1, v_out_dma_buf_addr, false);
}

void set_vga_scanlines_mode(bool sl_mode)
{
  scanlines_mode = sl_mode;
}

void start_vga(video_mode_t v_mode)
{
  video_mode = v_mode;

  int whole_line = video_mode.whole_line / video_mode.div;
  int h_sync_pulse_front = (video_mode.h_visible_area + video_mode.h_front_porch) / video_mode.div;
  int h_sync_pulse = video_mode.h_sync_pulse / video_mode.div;

  h_visible_area = video_mode.h_visible_area / (video_mode.div * 2);
  v_visible_area = V_BUF_H * video_mode.div;
  v_margin = (int16_t)((video_mode.v_visible_area - v_visible_area) / (video_mode.div * 2)) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;

  vreg_set_voltage(VREG_VOLTAGE_1_25);
  sleep_ms(100);
  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // palette initialization
  for (int i = 0; i < 16; i++)
    palette[i] = palette8[i] | (NO_SYNC ^ video_mode.sync_polality);

  // allocate memory for line template definitions
  uint8_t *base_ptr = calloc(whole_line * 4, sizeof(uint8_t));
  line_patterns[0] = (uint32_t *)base_ptr;

  // empty line
  memset(base_ptr, (NO_SYNC ^ video_mode.sync_polality), whole_line);
  memset(base_ptr + h_sync_pulse_front, (H_SYNC ^ video_mode.sync_polality), h_sync_pulse);

  // vertical sync
  base_ptr += whole_line;
  line_patterns[1] = (uint32_t *)base_ptr;
  memset(base_ptr, (V_SYNC ^ video_mode.sync_polality), whole_line);
  memset(base_ptr + h_sync_pulse_front, (VH_SYNC ^ video_mode.sync_polality), h_sync_pulse);

  //
  base_ptr += whole_line;
  line_patterns[2] = (uint32_t *)base_ptr;
  memcpy(base_ptr, line_patterns[0], whole_line);

  //
  base_ptr += whole_line;
  line_patterns[3] = (uint32_t *)base_ptr;
  memcpy(base_ptr, line_patterns[0], whole_line);

  // VGA pins
  for (int i = VGA_PIN_D0; i < VGA_PIN_D0 + 8; i++)
  {
    gpio_init(i);
    gpio_set_dir(i, GPIO_OUT);
    pio_gpio_init(PIO_VGA, i);
  }

  // PIO initialization
  // PIO program load
  uint offset = pio_add_program(PIO_VGA, &pio_program_vga);

  pio_sm_config c = pio_get_default_sm_config();

  pio_sm_set_consecutive_pindirs(PIO_VGA, SM_VGA, VGA_PIN_D0, 8, true);

  sm_config_set_wrap(&c, offset, offset + (pio_program_vga.length - 1));
  sm_config_set_out_shift(&c, true, true, 32);
  sm_config_set_out_pins(&c, VGA_PIN_D0, 8);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  sm_config_set_clkdiv(&c, ((float)clock_get_hz(clk_sys) * video_mode.div) / video_mode.pixel_freq);

  pio_sm_init(PIO_VGA, SM_VGA, offset, &c);
  pio_sm_set_enabled(PIO_VGA, SM_VGA, true);

  // DMA initialization
  int dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);

  // main (data) DMA channel
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);

  channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
  channel_config_set_read_increment(&c0, true);
  channel_config_set_write_increment(&c0, false);
  channel_config_set_dreq(&c0, DREQ_PIO_VGA + SM_VGA);
  channel_config_set_chain_to(&c0, dma_ch1); // chain to control channel

  dma_channel_configure(
      dma_ch0,
      &c0,
      &PIO_VGA->txf[SM_VGA], // write address
      line_patterns[0],      // read address
      whole_line / 4,        //
      false                  // don't start yet
  );

  // control DMA channel
  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);

  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_chain_to(&c1, dma_ch0); // chain to other channel

  dma_channel_configure(
      dma_ch1,
      &c1,
      &dma_hw->ch[dma_ch0].read_addr, // write address
      &line_patterns[0],              // read address
      1,                              //
      false                           // don't start yet
  );

  dma_channel_set_irq0_enabled(dma_ch1, true);

  // configure the processor to run dma_handler() when DMA IRQ 0 is asserted
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_vga);
  irq_set_enabled(DMA_IRQ_0, true);

  dma_start_channel_mask((1u << dma_ch0));
}
