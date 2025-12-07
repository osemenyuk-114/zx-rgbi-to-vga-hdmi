#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "g_config.h"
#include "vga.h"
#include "pio_programs.h"
#include "v_buf.h"

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"

static uint16_t osd_start_x;
static uint16_t osd_end_x;
static uint16_t osd_start_y;
static uint16_t osd_end_y;
static int osd_start_buf;
static int osd_end_buf;
#endif

extern settings_t settings;

static int dma_ch0;
static int dma_ch1;
static uint offset;

static video_mode_t video_mode;
static int16_t h_visible_area;
static int16_t h_margin;
static int16_t v_visible_area;
static int16_t v_margin;
static bool scanlines_mode = false;

static uint32_t *v_out_dma_buf[4];
static uint16_t palette[256];

void __not_in_flash_func(memset32)(uint32_t *dst, const uint32_t data, uint32_t size);

void __not_in_flash_func(dma_handler_vga)()
{
  static uint16_t y = 0;

  static uint8_t *scr_buffer = NULL;

  dma_hw->ints0 = 1u << dma_ch1;

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    scr_buffer = get_v_buf_out();
  }

  if (y >= video_mode.v_visible_area && y < (video_mode.v_visible_area + video_mode.v_front_porch))
  {
    // vertical sync front porch
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[0], false);
    return;
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  {
    // vertical sync pulse
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[1], false);
    return;
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse) && y < video_mode.whole_frame)
  {
    // vertical sync back porch
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[0], false);
    return;
  }

  if (!(scr_buffer))
  {
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[2], false);
    return;
  }

  // top and bottom black bars when the vertical size of the image is smaller than the vertical resolution of the screen
  if (y < v_margin || y >= (v_visible_area + v_margin))
  {
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[0], false);
    return;
  }

  // image area
  uint8_t line = y % (2 * video_mode.div);

  switch (video_mode.div)
  {
  case 2:
#ifdef SCANLINES_ENABLE_LOW_RES
    if (scanlines_mode)
    {
      if (line > 0)
        line++;

      if (line == 4)
        line++;
    }
    else if (line > 1)
      line++;

#else
    if (line > 1)
      line++;

#endif
    break;

  case 3:
    if (!scanlines_mode && ((line == 2) || (line == 5)))
      line--;
    break;

  case 4:
    if (scanlines_mode)
    {
#ifdef SCANLINES_USE_THIN
      if (line > 1)
        line--;

      if (line >= 5)
        line--;
#else
      if (line > 2)
        line--;

      if (line == 6)
        line--;
#endif
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

  default:
    break;
  }

  int active_buf_idx;

  switch (line)
  {
  case 0:
    active_buf_idx = 2;
    break;

  case 1:
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[2], false);
    return;

  case 2:
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[0], false);
    return;

  case 3:
    active_buf_idx = 3;
    break;

  case 4:
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[3], false);
    return;

  case 5:
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[0], false);
    return;

  default:
    return;
  }

  uint16_t scaled_y = (y - v_margin) / video_mode.div; // represents the line in the original captured image
  uint8_t *scr_line = &scr_buffer[scaled_y * (V_BUF_W / 2)];
  uint16_t *line_buf = (uint16_t *)v_out_dma_buf[active_buf_idx];

  // left margin
  for (int x = h_margin; x--;)
    *line_buf++ = palette[0];

#ifdef OSD_MENU_ENABLE
  // main image area with OSD compositing
  bool osd_active = osd_state.visible && (scaled_y >= osd_start_y && scaled_y < osd_end_y);

  if (osd_active)
  { // calculate OSD buffer line offset using scaled coordinates (2 pixels per byte)
    uint8_t *osd_line = &osd_buffer[(scaled_y - osd_start_y) * (OSD_WIDTH / 2)];

    int x = 0;

    // ultra-fast direct byte processing for pre-OSD area with loop unrolling
    while ((x + 4) <= osd_start_buf)
    {
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];

      x += 4;
    }

    while (x < osd_start_buf)
    {
      *line_buf++ = palette[*scr_line++];
      x++;
    }

    // ultra-simplified OSD compositing with optimized unrolling
    // process 4 bytes at a time for better performance
    while ((x + 4) <= osd_end_buf)
    {                                  // check if this entire 4-byte block is fully within OSD boundaries
      int screen_x_start = x << 1;     // first pixel of block
      int screen_x_end = (x + 3) << 1; // last pixel of block

      if (screen_x_start >= osd_start_x && (screen_x_end + 1) < osd_end_x)
      { // entire 4-byte block is OSD - direct OSD buffer processing
        *line_buf++ = palette[*osd_line++];
        *line_buf++ = palette[*osd_line++];
        *line_buf++ = palette[*osd_line++];
        *line_buf++ = palette[*osd_line++];

        scr_line += 4;
      }
      else
      { // block spans boundary - process individually
        for (int i = 0; i < 4; i++)
        {
          uint8_t scr_pixel = *scr_line++;
          uint8_t osd_pixel = *osd_line++;

          int pixel_x = (x + i) << 1;

          if (pixel_x >= osd_start_x && (pixel_x + 1) < osd_end_x)
            *line_buf++ = palette[osd_pixel];
          else
            *line_buf++ = palette[scr_pixel];
        }
      }

      x += 4;
    }

    while (x < osd_end_buf)
    { // handle remaining bytes (0-3 bytes)
      uint8_t scr_pixel = *scr_line++;
      uint8_t osd_pixel = *osd_line++;

      int screen_x_base = x << 1;

      if (screen_x_base >= osd_start_x && (screen_x_base + 1) < osd_end_x)
        *line_buf++ = palette[osd_pixel];
      else
        *line_buf++ = palette[scr_pixel];

      x++;
    }

    while ((x + 4) <= h_visible_area)
    {
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];

      x += 4;
    }

    while (x < h_visible_area)
    {
      *line_buf++ = palette[*scr_line++];
      x++;
    }
  }
  else
  { // ultra-fast direct byte processing for non-OSD area with loop unrolling
#endif
    int x = 0;

    while ((x + 4) <= h_visible_area)
    {
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];
      *line_buf++ = palette[*scr_line++];

      x += 4;
    }

    while (x < h_visible_area)
    {
      *line_buf++ = palette[*scr_line++];
      x++;
    }
#ifdef OSD_MENU_ENABLE
  }
#endif

  // right margin
  for (int x = h_margin; x--;)
    *line_buf++ = palette[0];

  dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[active_buf_idx], false);
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

  h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 2;
  h_margin = (h_visible_area - (uint8_t)(settings.frequency / 1000000) * (ACTIVE_VIDEO_TIME / 2)) / 2;

  if (h_margin < 0)
    h_margin = 0;

  h_visible_area -= h_margin * 2;

  v_visible_area = V_BUF_H * video_mode.div;
  v_margin = ((int16_t)((video_mode.v_visible_area - v_visible_area) / (video_mode.div * 2) + 0.5)) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;

#ifdef OSD_MENU_ENABLE
  osd_start_x = h_visible_area - OSD_WIDTH / 2;
  osd_end_x = osd_start_x + OSD_WIDTH;

  osd_start_y = ((video_mode.v_visible_area - 2 * v_margin) / video_mode.div - OSD_HEIGHT) / 2;
  osd_end_y = osd_start_y + OSD_HEIGHT;

  osd_start_buf = osd_start_x >> 1;
  osd_end_buf = (osd_end_x + 1) >> 1;

  // clamp to visible area
  if (osd_start_buf < 0)
    osd_start_buf = 0;

  if (osd_end_buf > h_visible_area)
    osd_end_buf = h_visible_area;

#endif

  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // palette initialization
  for (int i = 0; i < 16; i++)
  {
    uint8_t Yi = (i >> 3) & 1;
    uint8_t Ri = ((i >> 2) & 1) ? (Yi ? 0b00000011 : 0b00000010) : 0;
    uint8_t Gi = ((i >> 1) & 1) ? (Yi ? 0b00001100 : 0b00001000) : 0;
    uint8_t Bi = ((i >> 0) & 1) ? (Yi ? 0b00110000 : 0b00100000) : 0;

    for (int j = 0; j < 16; j++)
    {
      uint8_t Yj = (j >> 3) & 1;
      uint8_t Rj = ((j >> 2) & 1) ? (Yj ? 0b00000011 : 0b00000010) : 0;
      uint8_t Gj = ((j >> 1) & 1) ? (Yj ? 0b00001100 : 0b00001000) : 0;
      uint8_t Bj = ((j >> 0) & 1) ? (Yj ? 0b00110000 : 0b00100000) : 0;

      palette[(i * 16) + j] = ((uint16_t)(Ri | Gi | Bi | (NO_SYNC ^ video_mode.sync_polarity)) << 8) | (Rj | Gj | Bj | (NO_SYNC ^ video_mode.sync_polarity));
    }
  }

  // set VGA pins
  for (int i = VGA_PIN_D0; i < VGA_PIN_D0 + 8; i++)
  {
    pio_gpio_init(PIO_VGA, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_SLOW);
  }

  // allocate memory for line template definitions - individual allocations
  // empty line
  v_out_dma_buf[0] = calloc(whole_line / 4, sizeof(uint32_t));
  memset((uint8_t *)v_out_dma_buf[0], (NO_SYNC ^ video_mode.sync_polarity), whole_line);
  memset((uint8_t *)v_out_dma_buf[0] + h_sync_pulse_front, (H_SYNC ^ video_mode.sync_polarity), h_sync_pulse);
  // vertical sync pulse
  v_out_dma_buf[1] = calloc(whole_line / 4, sizeof(uint32_t));
  memset((uint8_t *)v_out_dma_buf[1], (V_SYNC ^ video_mode.sync_polarity), whole_line);
  memset((uint8_t *)v_out_dma_buf[1] + h_sync_pulse_front, (VH_SYNC ^ video_mode.sync_polarity), h_sync_pulse);
  // image line
  v_out_dma_buf[2] = calloc(whole_line / 4, sizeof(uint32_t));
  memcpy((uint8_t *)v_out_dma_buf[2], (uint8_t *)v_out_dma_buf[0], whole_line);
  // image line
  v_out_dma_buf[3] = calloc(whole_line / 4, sizeof(uint32_t));
  memcpy((uint8_t *)v_out_dma_buf[3], (uint8_t *)v_out_dma_buf[0], whole_line);

  // PIO initialization
  pio_sm_config c = pio_get_default_sm_config();

  // PIO program load
  offset = pio_add_program(PIO_VGA, &pio_vga_program);
  sm_config_set_wrap(&c, offset, offset + (pio_vga_program.length - 1));

  sm_config_set_out_pins(&c, VGA_PIN_D0, 8);
  pio_sm_set_consecutive_pindirs(PIO_VGA, SM_VGA, VGA_PIN_D0, 8, true);

  sm_config_set_out_shift(&c, true, true, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  sm_config_set_clkdiv(&c, ((float)clock_get_hz(clk_sys) * video_mode.div) / video_mode.pixel_freq);

  pio_sm_init(PIO_VGA, SM_VGA, offset, &c);
  pio_sm_set_enabled(PIO_VGA, SM_VGA, true);

  // DMA initialization
  dma_ch0 = dma_claim_unused_channel(true);
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
      v_out_dma_buf[0],      // read address
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
      &v_out_dma_buf[0],              // read address
      1,                              //
      false                           // don't start yet
  );

  dma_channel_set_irq0_enabled(dma_ch1, true);

  // configure the processor to run dma_handler() when DMA IRQ 0 is asserted
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_vga);
  irq_set_enabled(DMA_IRQ_0, true);

  dma_start_channel_mask((1u << dma_ch0));
}

void stop_vga()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_0, false);

  // clear the IRQ handler to prevent conflicts with DVI
  irq_remove_handler(DMA_IRQ_0, dma_handler_vga);

  // stop PIO
  pio_sm_set_enabled(PIO_VGA, SM_VGA, false);
  pio_sm_init(PIO_VGA, SM_VGA, offset, NULL);
  pio_remove_program(PIO_VGA, &pio_vga_program, offset);

  // cleanup and free DMA channels
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);

  // free individual buffer allocations
  if (v_out_dma_buf[0] != NULL)
  {
    free(v_out_dma_buf[0]);
    v_out_dma_buf[0] = NULL;
  }

  if (v_out_dma_buf[1] != NULL)
  {
    free(v_out_dma_buf[1]);
    v_out_dma_buf[1] = NULL;
  }

  if (v_out_dma_buf[2] != NULL)
  {
    free(v_out_dma_buf[2]);
    v_out_dma_buf[2] = NULL;
  }

  if (v_out_dma_buf[3] != NULL)
  {
    free(v_out_dma_buf[3]);
    v_out_dma_buf[3] = NULL;
  }
}