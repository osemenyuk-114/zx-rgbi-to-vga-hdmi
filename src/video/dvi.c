#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "g_config.h"
#include "dvi.h"
#include "pio_programs.h"
#include "v_buf.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

extern settings_t settings;

static int dma_ch0; // data line → conv PIO TX
static int dma_ch1; // control: reloads ch0 read addr, fires IRQ
static int dma_ch2; // out_data: reads palette data → sends to output PIO TX
static int dma_ch3; // set_addr: reads address from conv RX → sets ch2 read addr
static uint offset;
static uint offset_conv;

extern video_mode_t video_mode;
extern int16_t h_visible_area;

static uint32_t *v_out_dma_buf[2];
static uint32_t *v_out_sync_hblank; // pre-filled H-blank line (NO_SYNC + H_SYNC + NO_SYNC)
static uint32_t *v_out_sync_vsync;  // pre-filled V-sync line (V_SYNC + VH_SYNC + V_SYNC)

static uint32_t pixels[256];

// 4KB-aligned palette: 20 entries × 16 bytes (4 × uint32_t each)
// Each entry: {normal_lo, normal_hi, inverted_lo, inverted_hi}
// Entries 0-15: colors, 16-19: sync patterns
static uint32_t palette[20 * 4] __attribute__((aligned(4096)));
// sync pulse pattern indexes (after 16 color entries)
static const uint8_t NO_SYNC = 16;
static const uint8_t H_SYNC = 17;
static const uint8_t V_SYNC = 18;
static const uint8_t VH_SYNC = 19;

static uint64_t get_ser_diff_data(uint16_t dataR, uint16_t dataG, uint16_t dataB)
{
  uint64_t out64 = 0;
  uint8_t d6;
  uint8_t bR;
  uint8_t bG;
  uint8_t bB;

  for (int bit = 9; bit >= 0; bit--)
  {
    out64 <<= 6;

    if (bit == 4)
      out64 <<= 2;

    bR = (dataR >> bit) & 1;
    bG = (dataG >> bit) & 1;
    bB = (dataB >> bit) & 1;

#ifndef DVI_PINS_REVERSED
    bR = 2 - bR;
    bG = 2 - bG;
    bB = 2 - bB;

    d6 = (bB << 4) | (bG << 2) | (bR << 0);
#else
    bR = bR + 1;
    bG = bG + 1;
    bB = bB + 1;

    d6 = (bR << 4) | (bG << 2) | (bB << 0);
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

// Load a 32-bit value into PIO X register (SM must be stopped or idle)
static void pio_set_x(PIO pio, int sm, uint32_t v)
{
  uint instr_shift = pio_encode_in(pio_x, 4);
  uint instr_mov = pio_encode_mov(pio_x, pio_isr);
  for (int i = 0; i < 8; i++)
  {
    const uint32_t nibble = (v >> (i * 4)) & 0xf;
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, nibble));
    pio_sm_exec(pio, sm, instr_shift);
  }
  pio_sm_exec(pio, sm, instr_mov);
}

static void __not_in_flash_func(dma_handler_dvi)()
{
  static uint16_t y = 0;

  static uint8_t *scr_buffer = NULL;
  static uint32_t active_buf_idx = 0;

  dma_hw->ints0 = 1u << dma_ch1;

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    scr_buffer = get_v_buf_out();
  }

  if (y < video_mode.v_visible_area)
  { // visible area — use ping-pong buffers (H sync pre-filled at init)
    dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[active_buf_idx & 1], false);

    if (y & 1)
      return;

    active_buf_idx++;

    uint32_t *active_buf = v_out_dma_buf[active_buf_idx & 1];

    if (scr_buffer == NULL)
      return;

    uint16_t scaled_y = y / video_mode.div;
    uint8_t *scr_line = &scr_buffer[scaled_y * (V_BUF_W / 2)];
    uint32_t *line_buf = active_buf;

#ifdef OSD_ENABLE
    // check if OSD is visible and overlaps with current scaled scanline
    bool osd_active = osd_state.visible && (scaled_y >= osd_mode.start_y && scaled_y < osd_mode.end_y);

    if (osd_active)
    { // calculate OSD buffer line offset using scaled coordinates (2 pixels per byte)
      uint8_t *osd_line = &osd_buffer[(scaled_y - osd_mode.start_y) * (osd_mode.width / 2)];

      int x = 0;

      if (!osd_mode.full_width)
      {
        for (; (x + 4) <= osd_mode.start_x; x += 4)
        {
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
        }

        for (; x < osd_mode.start_x; x++)
          *line_buf++ = pixels[*scr_line++];
      }
      else
        for (; x < osd_mode.start_x; x++)
        {
          scr_line++;
          *line_buf++ = pixels[0]; // black pixels
        }

      for (; (x + 4) <= osd_mode.end_x; x += 4)
      {
        scr_line += 4;

        *line_buf++ = pixels[*osd_line++];
        *line_buf++ = pixels[*osd_line++];
        *line_buf++ = pixels[*osd_line++];
        *line_buf++ = pixels[*osd_line++];
      }

      for (; x < osd_mode.end_x; x++)
      {
        scr_line++;
        *line_buf++ = pixels[*osd_line++];
      }

      if (!osd_mode.full_width)
      {
        for (; (x + 4) <= h_visible_area; x += 4)
        {
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
          *line_buf++ = pixels[*scr_line++];
        }

        for (; x < h_visible_area; x++)
          *line_buf++ = pixels[*scr_line++];
      }
      else
        for (; x < h_visible_area; x++)
        {
          scr_line++;
          *line_buf++ = pixels[0]; // black pixels
        }
    }
    else
#endif
    {
      int x = 0;

      for (; (x + 4) <= h_visible_area; x += 4)
      {
        *line_buf++ = pixels[*scr_line++];
        *line_buf++ = pixels[*scr_line++];
        *line_buf++ = pixels[*scr_line++];
        *line_buf++ = pixels[*scr_line++];
      }

      for (; x < h_visible_area; x++)
        *line_buf++ = pixels[*scr_line++];
    }
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  { // V sync — use pre-filled buffer
    dma_channel_set_read_addr(dma_ch1, &v_out_sync_vsync, false);
  }
  else
  { // H blank (front/back porch) — use pre-filled buffer
    dma_channel_set_read_addr(dma_ch1, &v_out_sync_hblank, false);
  }
}

void start_dvi()
{
  int whole_line = video_mode.whole_line;

  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // pixels[] lookup: each input byte (2 packed 4-bit pixels) → 2 palette indices
  // byte order (LSB first): p1_color, p2_color (each palette entry has norm+inv)
  for (int i = 0; i < 256; i++)
  {
    uint8_t p1 = i & 0x0f;
    uint8_t p2 = (i >> 4) & 0x0f;

    pixels[i] = (p2 << 8) | p1;
  }

  // TMDS control character constants
  const uint16_t b0 = 0b1101010100;
  const uint16_t b1 = 0b0010101011;
  const uint16_t b2 = 0b0101010100;
  const uint16_t b3 = 0b1010101011;

  // sync palette entries (16 bytes each: norm_lo, norm_hi, norm_lo, norm_hi)
  uint64_t sync_val;
  sync_val = get_ser_diff_data(b0, b0, b3);
  palette[NO_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[NO_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[NO_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[NO_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b2);
  palette[H_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[H_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[H_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[H_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b1);
  palette[V_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[V_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[V_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[V_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b0);
  palette[VH_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[VH_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[VH_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[VH_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);

  // color palette: 16 entries × 16 bytes {norm_lo, norm_hi, inv_lo, inv_hi}
  for (int c = 0; c < 16; c++)
  {
    uint8_t Y = (c >> 3) & 1;
    uint8_t R = ((c >> 2) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t G = ((c >> 1) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t B = ((c >> 0) & 1) ? (Y ? 255 : 170) : 0;
    uint64_t normal = get_ser_diff_data(tmds_encoder(R), tmds_encoder(G), tmds_encoder(B));
    uint64_t inverted = normal ^ 0x0003ffffffffffffl;
    palette[c * 4 + 0] = (uint32_t)(normal);
    palette[c * 4 + 1] = (uint32_t)(normal >> 32);
    palette[c * 4 + 2] = (uint32_t)(inverted);
    palette[c * 4 + 3] = (uint32_t)(inverted >> 32);
  }

  // set DVI data pins
  for (int i = DVI_PIN_D0; i < DVI_PIN_D0 + 6; i++)
  {
    pio_gpio_init(PIO_DVI, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // set DVI clock pins
  for (int i = DVI_PIN_CLK0; i < DVI_PIN_CLK0 + 2; i++)
  {
    pio_gpio_init(PIO_DVI, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // allocate sync line buffers (pre-filled, never modified)
  v_out_sync_hblank = calloc(whole_line, sizeof(uint8_t));
  memset((uint8_t *)v_out_sync_hblank, NO_SYNC, video_mode.h_visible_area + video_mode.h_front_porch);
  memset((uint8_t *)v_out_sync_hblank + video_mode.h_visible_area + video_mode.h_front_porch, H_SYNC, video_mode.h_sync_pulse);
  memset((uint8_t *)v_out_sync_hblank + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, NO_SYNC, video_mode.h_back_porch);

  v_out_sync_vsync = calloc(whole_line, sizeof(uint8_t));
  memset((uint8_t *)v_out_sync_vsync, V_SYNC, video_mode.h_visible_area + video_mode.h_front_porch);
  memset((uint8_t *)v_out_sync_vsync + video_mode.h_visible_area + video_mode.h_front_porch, VH_SYNC, video_mode.h_sync_pulse);
  memset((uint8_t *)v_out_sync_vsync + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, V_SYNC, video_mode.h_back_porch);

  // allocate image line buffers (ping-pong, pre-filled with H-blank sync pattern)
  v_out_dma_buf[0] = calloc(whole_line, sizeof(uint8_t));
  memcpy((uint8_t *)v_out_dma_buf[0], (uint8_t *)v_out_sync_hblank, whole_line);

  v_out_dma_buf[1] = calloc(whole_line, sizeof(uint8_t));
  memcpy((uint8_t *)v_out_dma_buf[1], (uint8_t *)v_out_sync_hblank, whole_line);

  // === Output PIO (SM0): TMDS serializer ===
  pio_sm_config c = pio_get_default_sm_config();

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

  // === Conv PIO (SM1): index → palette address converter ===
  pio_sm_config c_conv = pio_get_default_sm_config();

  offset_conv = pio_add_program(PIO_DVI, &pio_dvi_conv_program);
  sm_config_set_wrap(&c_conv, offset_conv, offset_conv + pio_dvi_conv_program.length - 1);
  sm_config_set_in_shift(&c_conv, true, false, 32);  // shift right, no autopush
  sm_config_set_out_shift(&c_conv, true, false, 32); // shift right, no autopull (explicit pull used)

  pio_sm_clear_fifos(PIO_DVI, SM_DVI_CONV);
  pio_sm_restart(PIO_DVI, SM_DVI_CONV);

  // load palette base address >> 12 into X register (4KB-aligned, 16-byte entries)
  pio_set_x(PIO_DVI, SM_DVI_CONV, ((uint32_t)palette) >> 12);

  pio_sm_init(PIO_DVI, SM_DVI_CONV, offset_conv, &c_conv);
  pio_sm_set_enabled(PIO_DVI, SM_DVI_CONV, true);

  // === DMA initialization (4 channels) ===
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);
  dma_ch2 = dma_claim_unused_channel(true);
  dma_ch3 = dma_claim_unused_channel(true);

  // ch0: data line → conv PIO TX (feeds index buffer to converter)
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);
  channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
  channel_config_set_read_increment(&c0, true);
  channel_config_set_write_increment(&c0, false);
  channel_config_set_dreq(&c0, DREQ_PIO_DVI + SM_DVI_CONV); // conv SM TX
  channel_config_set_chain_to(&c0, dma_ch1);

  dma_channel_configure(
      dma_ch0,
      &c0,
      &PIO_DVI->txf[SM_DVI_CONV], // write: conv PIO TX FIFO
      &v_out_dma_buf[0][0],       // read: index buffer
      whole_line / 4,             // transfer count: bytes / 4
      false                       // don't start yet
  );

  // ch1: control — reloads ch0 read addr, fires IRQ
  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);
  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_chain_to(&c1, dma_ch0);

  dma_channel_configure(
      dma_ch1,
      &c1,
      &dma_hw->ch[dma_ch0].read_addr, // write: ch0's read addr
      &v_out_dma_buf[0],              // read: pointer to buffer
      1,
      false // don't start yet
  );

  // ch2: out_data — reads palette entry → sends TMDS data to output PIO
  // NOTE: ch2 MUST be configured before ch3, because ch3 chains to ch2
  dma_channel_config c2 = dma_channel_get_default_config(dma_ch2);
  channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
  channel_config_set_read_increment(&c2, true);
  channel_config_set_write_increment(&c2, false);
  channel_config_set_dreq(&c2, DREQ_PIO_DVI + SM_DVI); // output SM TX
  channel_config_set_chain_to(&c2, dma_ch3);

  dma_channel_configure(
      dma_ch2,
      &c2,
      &PIO_DVI->txf[SM_DVI], // write: output PIO TX FIFO
      palette,               // read: palette (overwritten by ch3 each cycle)
      4,                     // 4 × uint32_t = 16 bytes per palette entry (norm + inv)
      false                  // don't start yet
  );

  // ch3: set_addr — reads palette address from conv RX → sets ch2 read addr
  dma_channel_config c3 = dma_channel_get_default_config(dma_ch3);
  channel_config_set_transfer_data_size(&c3, DMA_SIZE_32);
  channel_config_set_read_increment(&c3, false);
  channel_config_set_write_increment(&c3, false);
  channel_config_set_dreq(&c3, DREQ_PIO0_RX0 + SM_DVI_CONV); // conv SM RX
  channel_config_set_chain_to(&c3, dma_ch2);

  dma_channel_configure(
      dma_ch3,
      &c3,
      &dma_hw->ch[dma_ch2].read_addr, // write: ch2's read addr
      &PIO_DVI->rxf[SM_DVI_CONV],     // read: conv RX FIFO
      1,
      true // start immediately — ch2 is already configured
  );

  // IRQ setup
  dma_channel_set_irq0_enabled(dma_ch1, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_dvi);
  irq_set_priority(DMA_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
  irq_set_enabled(DMA_IRQ_0, true);

  // start the line DMA (ch3↔ch2 loop is already running)
  dma_start_channel_mask((1u << dma_ch0));
}

void stop_dvi()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_0, false);
  irq_remove_handler(DMA_IRQ_0, dma_handler_dvi);

  // stop output PIO (SM0)
  pio_sm_set_enabled(PIO_DVI, SM_DVI, false);
  pio_sm_init(PIO_DVI, SM_DVI, offset, NULL);
  pio_remove_program(PIO_DVI, &pio_dvi_program, offset);

  // stop conv PIO (SM1)
  pio_sm_set_enabled(PIO_DVI, SM_DVI_CONV, false);
  pio_sm_init(PIO_DVI, SM_DVI_CONV, offset_conv, NULL);
  pio_remove_program(PIO_DVI, &pio_dvi_conv_program, offset_conv);

  // cleanup and free all 4 DMA channels
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_cleanup(dma_ch2);
  dma_channel_cleanup(dma_ch3);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);
  dma_channel_unclaim(dma_ch2);
  dma_channel_unclaim(dma_ch3);

  // free index buffers
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

  // free sync buffers
  if (v_out_sync_hblank != NULL)
  {
    free(v_out_sync_hblank);
    v_out_sync_hblank = NULL;
  }

  if (v_out_sync_vsync != NULL)
  {
    free(v_out_sync_vsync);
    v_out_sync_vsync = NULL;
  }
}
