#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "g_config.h"
#include "dvi.h"
#include "pio_programs.h"
#include "v_buf.h"

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"

static uint16_t osd_start_x;
static uint16_t osd_end_x;
static uint16_t osd_start_y;
static uint16_t osd_end_y;
#endif

extern settings_t settings;

static int dma_ch0;
static int dma_ch1;
static uint offset;

static video_mode_t video_mode;
static int16_t h_visible_area;

static uint32_t *v_out_dma_buf[2];

static uint64_t sync_data[4];
static uint64_t R64, G64, B64, Y64;
static uint64_t palette[32];

static void __not_in_flash_func(memset64)(uint64_t *dst, const uint64_t data, uint32_t size)
{
  uint64_t *end = dst + size;
  while (dst < end)
    *dst++ = data;
}

static uint64_t get_ser_diff_data(uint16_t dataR, uint16_t dataG, uint16_t dataB)
{
  uint64_t out64 = 0;
  uint8_t d6;
  uint8_t bR;
  uint8_t bG;
  uint8_t bB;

  for (int i = 0; i < 10; i++)
  {
    out64 <<= 6;

    if (i == 5)
      out64 <<= 2;

    bR = (dataR >> (9 - i)) & 1;
    bG = (dataG >> (9 - i)) & 1;
    bB = (dataB >> (9 - i)) & 1;

    bR |= (bR ^ 1) << 1;
    bG |= (bG ^ 1) << 1;
    bB |= (bB ^ 1) << 1;

#ifdef DVI_PIN_invert_diffpairs
    bR ^= 0b11;
    bG ^= 0b11;
    bB ^= 0b11;
#endif

#ifdef DVI_PIN_RGB_notBGR
    d6 = (bR << 4) | (bG << 2) | (bB << 0);
#else
    d6 = (bB << 4) | (bG << 2) | (bR << 0);
#endif

    out64 |= d6;
  }

  return out64;
}

// TMDS encoder
static uint tmds_encoder(uint8_t d8)
{
  int s1 = 0;
  uint8_t xnor = 0;

  for (int i = 0; i < 8; i++)
    s1 += (d8 & (1u << i)) ? 1 : 0;

  if ((s1 > 4) || ((s1 == 4) && ((d8 & 1) == 0)))
    xnor = 1;

  uint16_t d_out = d8 & 1;
  uint16_t qi = d_out;

  for (int i = 1; i < 8; i++)
  {
    d_out |= ((qi << 1) ^ (d8 & (1u << i))) ^ (xnor << i);
    qi = d_out & (1u << i);
  }

  if (xnor == 1)
    d_out |= 1u << 9;
  else
    d_out |= 1u << 8;

  return d_out;
}

static void __not_in_flash_func(dma_handler_dvi)()
{
  static uint16_t y = 0;

  static uint8_t *scr_buffer = NULL;
  static uint32_t active_buf_idx = 0;

  dma_hw->ints0 = 1u << dma_ch1;

  dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[active_buf_idx & 1], false);

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    scr_buffer = get_v_buf_out();
  }

  if (y & 1)
    return;

  active_buf_idx++;

  uint64_t *active_buf = (uint64_t *)(v_out_dma_buf[active_buf_idx & 1]);

  if (scr_buffer == NULL)
    return;

  if (y < video_mode.v_visible_area)
  { // image area
    uint16_t scaled_y = y / video_mode.div;
    uint8_t *scr_line = &scr_buffer[scaled_y * (V_BUF_W / 2)];
    uint64_t *line_buf = active_buf;

#ifdef OSD_MENU_ENABLE
    // check if OSD is visible and overlaps with current scaled scanline
    bool osd_active = osd_state.visible && (scaled_y >= osd_start_y && scaled_y < osd_end_y);

    if (osd_active)
    { // calculate OSD buffer line offset using scaled coordinates (2 pixels per byte)
      uint8_t *osd_line = &osd_buffer[(scaled_y - osd_start_y) * (OSD_WIDTH / 2)];

      int x = 0;

      while (x < osd_start_x)
      { // fast loop for pre-OSD area (no OSD checks) - optimized palette access
        uint8_t c2 = *scr_line++;
        uint8_t pixel1 = c2 & 0xf;
        uint8_t pixel2 = c2 >> 4;

        uint64_t *palette_ptr = &palette[pixel1 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        palette_ptr = &palette[pixel2 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        x++;
      }

      while (x < osd_end_x)
      { // ultra-simplified OSD compositing - byte-aligned boundaries (2-pixel aligned)
        scr_line++;
        uint8_t o2 = *osd_line++;
        uint8_t pixel1 = o2 & 0xf;
        uint8_t pixel2 = o2 >> 4;

        uint64_t *palette_ptr = &palette[pixel1 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        palette_ptr = &palette[pixel2 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        x++;
      }

      while (x < h_visible_area)
      { // fast loop for post-OSD area (no OSD checks) - optimized palette access
        uint8_t c2 = *scr_line++;
        uint8_t pixel1 = c2 & 0xf;
        uint8_t pixel2 = c2 >> 4;

        uint64_t *palette_ptr = &palette[pixel1 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        palette_ptr = &palette[pixel2 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        x++;
      }
    }
    else
#endif
      for (int x = 0; x < h_visible_area; x++)
      { // no OSD - maximum speed path
        uint8_t c2 = *scr_line++;
        uint8_t pixel1 = c2 & 0xf;
        uint8_t pixel2 = c2 >> 4;

        uint64_t *palette_ptr = &palette[pixel1 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;

        palette_ptr = &palette[pixel2 << 1];
        *line_buf++ = *palette_ptr++;
        *line_buf++ = *palette_ptr;
      }

    // horizontal sync
    memset64(active_buf + video_mode.h_visible_area, sync_data[0b00], video_mode.h_front_porch);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch, sync_data[0b01], video_mode.h_sync_pulse);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, sync_data[0b00], video_mode.h_back_porch);
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  {
    // vertical sync pulse
    memset64(active_buf, sync_data[0b10], video_mode.h_visible_area + video_mode.h_front_porch);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch, sync_data[0b11], video_mode.h_sync_pulse);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, sync_data[0b10], video_mode.h_back_porch);
  }
  else
  {
    // vertical sync back porch
    memset64(active_buf, sync_data[0b00], video_mode.h_visible_area + video_mode.h_front_porch);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch, sync_data[0b01], video_mode.h_sync_pulse);
    memset64(active_buf + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, sync_data[0b00], video_mode.h_back_porch);
  }
}

void start_dvi(video_mode_t v_mode)
{
  video_mode = v_mode;

  int whole_line = video_mode.whole_line * video_mode.div;

  h_visible_area = video_mode.h_visible_area / (2 * video_mode.div);

#ifdef OSD_MENU_ENABLE
  osd_start_x = (h_visible_area - OSD_WIDTH / 2) / 2;
  osd_end_x = osd_start_x + OSD_WIDTH / 2;

  osd_start_y = (video_mode.v_visible_area / video_mode.div - OSD_HEIGHT) / 2;
  osd_end_y = osd_start_y + OSD_HEIGHT;
#endif

  // initialization of constants
  const uint16_t b0 = 0b1101010100;
  const uint16_t b1 = 0b0010101011;
  const uint16_t b2 = 0b0101010100;
  const uint16_t b3 = 0b1010101011;

  sync_data[0b00] = get_ser_diff_data(b0, b0, b3);
  sync_data[0b01] = get_ser_diff_data(b0, b0, b2);
  sync_data[0b10] = get_ser_diff_data(b0, b0, b1);
  sync_data[0b11] = get_ser_diff_data(b0, b0, b0);

  R64 = get_ser_diff_data(tmds_encoder(255), tmds_encoder(0), tmds_encoder(0));
  G64 = get_ser_diff_data(tmds_encoder(0), tmds_encoder(255), tmds_encoder(0));
  B64 = get_ser_diff_data(tmds_encoder(0), tmds_encoder(0), tmds_encoder(255));
  Y64 = get_ser_diff_data(tmds_encoder(255), tmds_encoder(255), tmds_encoder(0));

  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // palette initialization
  for (int c = 0; c < 16; c++)
  {
    uint8_t Y = (c >> 3) & 1;
    uint8_t R = ((c >> 2) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t G = ((c >> 1) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t B = ((c >> 0) & 1) ? (Y ? 255 : 170) : 0;
    palette[c * 2] = get_ser_diff_data(tmds_encoder(R), tmds_encoder(G), tmds_encoder(B));
    palette[c * 2 + 1] = palette[c * 2] ^ 0x0003ffffffffffffl;
  }

  // set DVI pins
  for (int i = DVI_PIN_D0; i < DVI_PIN_D0 + 8; i++)
  {
    pio_gpio_init(PIO_DVI, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // buffers initialization
  v_out_dma_buf[0] = calloc(whole_line, sizeof(uint32_t));
  v_out_dma_buf[1] = calloc(whole_line, sizeof(uint32_t));

  // PIO initialization
  pio_sm_config c = pio_get_default_sm_config();

  // PIO program load
  offset = pio_add_program(PIO_DVI, &pio_dvi_program);
  sm_config_set_wrap(&c, offset, offset + pio_dvi_program.length - 1);

  sm_config_set_out_pins(&c, DVI_PIN_D0, 6);
  pio_sm_set_consecutive_pindirs(PIO_DVI, SM_DVI, DVI_PIN_D0, 6, true);

  pio_sm_set_pins_with_mask(PIO_DVI, SM_DVI, 3u << DVI_PIN_CLK0, 3u << DVI_PIN_CLK0);
  pio_sm_set_pindirs_with_mask(PIO_DVI, SM_DVI, 3u << DVI_PIN_CLK0, 3u << DVI_PIN_CLK0);

  sm_config_set_sideset_pins(&c, DVI_PIN_CLK0);
  sm_config_set_sideset(&c, 2, false, false);

  sm_config_set_out_shift(&c, true, true, 30);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  pio_sm_init(PIO_DVI, SM_DVI, offset, &c);
  pio_sm_set_enabled(PIO_DVI, SM_DVI, true);

  // DMA initialization
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);

  // main (data) DMA channel
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);

  channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
  channel_config_set_read_increment(&c0, true);
  channel_config_set_write_increment(&c0, false);
  channel_config_set_dreq(&c0, DREQ_PIO_DVI + SM_DVI);
  channel_config_set_chain_to(&c0, dma_ch1); // chain to control channel

  dma_channel_configure(
      dma_ch0,
      &c0,
      &PIO_DVI->txf[SM_DVI], // write address
      &v_out_dma_buf[0][0],  // read address
      whole_line,            //
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
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_dvi);
  irq_set_enabled(DMA_IRQ_0, true);

  dma_start_channel_mask((1u << dma_ch0));
}

void stop_dvi()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_0, false);

  // clear the IRQ handler to prevent conflicts with VGA
  irq_remove_handler(DMA_IRQ_0, dma_handler_dvi);

  // stop PIO
  pio_sm_set_enabled(PIO_DVI, SM_DVI, false);
  pio_sm_init(PIO_DVI, SM_DVI, offset, NULL);
  pio_remove_program(PIO_DVI, &pio_dvi_program, offset);

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
}
