#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/hstx_ctrl.h"

#include "g_config.h"
#include "dvi.h"
#include "v_buf.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

#ifdef HDMI_AUDIO_ENABLE
#include "hdmi_audio.h"
#endif

extern settings_t settings;

static int dma_ch0;
static int dma_ch1;

extern video_mode_t video_mode;
extern int16_t h_visible_area;
extern int16_t h_margin;

static uint32_t *v_out_dma_buf[2];
static uint32_t *v_out_dma_buf_alloc; // single contiguous allocation for both ping-pong buffers

static uint8_t *scr_buffer = NULL;
static uint32_t active_buf_idx = 0;

static uint32_t pixels[256];

// TMDS 10-bit control character symbols
#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu
// 30-bit TMDS sync words (R=G=CTRL_00, B=sync); built from sync_polarity at runtime
static uint32_t SYNC_V0_H0, SYNC_V0_H1, SYNC_V1_H0, SYNC_V1_H1;
// HSTX command expander opcodes (bits [15:12] of command word)
#define HSTX_CMD_RAW (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT (0x1u << 12)
#define HSTX_CMD_TMDS (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP (0xfu << 12)

#ifdef HDMI_AUDIO_ENABLE
// HDMI "video guard band": 2 fixed TERC4 symbols that MUST immediately precede
// the active video period whenever a data island occurred earlier in the same
// line (HDMI spec 5.2.3.3). Distinct from the data island's own guard band.
#define VIDEO_GUARD_CH0 0x2ccu  // channel 0 (blue)
#define VIDEO_GUARD_CH12 0x133u // channels 1,2 (green, red)
static uint32_t VIDEO_GUARD_WORD;
// Number of HSTX command words prepended to each active-line pixel buffer.
// With HDMI audio enabled the data island is placed inside the HSYNC pulse
// (the back porch is too short in 640x480), laid out as:
//   front porch | HSYNC: 4 ctrl px, 8 data-island preamble px, 36 island px
//   (2 guard + 32 packet + 2 guard), remaining HSYNC | back porch ctrl |
//   8 video preamble px + 2 video guard px | active video
// Preambles are mandatory (HDMI 1.4a 5.2.1.1); without them sinks cannot
// find the island/video start and show vertical colour bars or no picture.
// Without audio the prefi
#define HDMI_PREAMBLE_PIXELS 8
#define HDMI_ISLAND_LEAD_CTRL 4 // control pixels between HSYNC start and the island preamble
#define HDMI_ISLAND_SEGMENT_PIXELS (HDMI_ISLAND_LEAD_CTRL + HDMI_PREAMBLE_PIXELS + HDMI_AUDIO_ISLAND_WORDS)
#define HDMI_VIDEO_LEAD_PIXELS (HDMI_PREAMBLE_PIXELS + 2) // video preamble + video guard band
// Shared horizontal-blanking prefix (front porch, HSYNC with island): word count and island index
#define HBLANK_PREFIX_WORDS 51
#define HBLANK_ISLAND_OFFSET 13 // index of the first of the 36 island words
#define VACTIVE_CMD_WORDS (HBLANK_PREFIX_WORDS + 3 + HDMI_VIDEO_LEAD_PIXELS + 1)
#define VACTIVE_ISLAND_OFFSET HBLANK_ISLAND_OFFSET
// HSTX command lists for vblank lines, double-buffered (2 slots each) so a
// fresh data island can be written into the slot NOT currently in flight on
// the chained DMA channel (see dma_handler_dvi)
#define VBLANK_LINE_WORDS (HBLANK_PREFIX_WORDS + 3)
#define VBLANK_ISLAND_OFFSET HBLANK_ISLAND_OFFSET
static uint32_t vblank_line_vsync_off[2][VBLANK_LINE_WORDS];
static uint32_t vblank_line_vsync_on[2][VBLANK_LINE_WORDS];
static uint32_t vblank_off_toggle = 0;
static uint32_t vblank_on_toggle = 0;

// Fills the front porch + HSYNC (with data island) command prefix.
// `h0`/`h1` are the HSYNC idle/active sync words for the line's VSYNC state.
// Returns words written.
static int fill_hblank_island_prefix(uint32_t *p, uint32_t h0, uint32_t h1)
{
  uint32_t preamble = (h1 & 0x3ffu) | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20); // CTL0=1, CTL1=0, CTL2=1, CTL3=0
  int n = 0;

  p[n++] = HSTX_CMD_RAW_REPEAT | video_mode.h_front_porch;
  p[n++] = h0;
  p[n++] = HSTX_CMD_RAW_REPEAT | HDMI_ISLAND_LEAD_CTRL;
  p[n++] = h1;
  p[n++] = HSTX_CMD_RAW | (HDMI_PREAMBLE_PIXELS + HDMI_AUDIO_ISLAND_WORDS);

  for (int i = 0; i < HDMI_PREAMBLE_PIXELS; i++)
    p[n++] = preamble;

  n += HDMI_AUDIO_ISLAND_WORDS; // island words are refreshed per line
  p[n++] = HSTX_CMD_RAW_REPEAT | (video_mode.h_sync_pulse - HDMI_ISLAND_SEGMENT_PIXELS);
  p[n++] = h1;

  return n;
}
#else
#define VACTIVE_CMD_WORDS 9
// HSTX command lists for vblank lines
static uint32_t vblank_line_vsync_off[8];
static uint32_t vblank_line_vsync_on[8];
#endif

static uint16_t v_out_dma_buf_words = 0;  // words per line buffer (margins + content)
static uint16_t v_out_dma_line_words = 0; // words per full-line buffer (cmd prefix + pixels)

// ISR state (file-scope for reset in stop_dvi)
static uint16_t y = 1; // HSTX: IRQ fires at end-of-line, y starts at 1

static void __not_in_flash_func(dma_handler_dvi)()
{
  uint ch_num = (dma_hw->ints0 & (1u << dma_ch0)) ? dma_ch0 : dma_ch1;
  dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
  dma_hw->ints0 = 1u << ch_num;

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    scr_buffer = get_v_buf_out();
    active_buf_idx = 0;
  }

  if (y < video_mode.v_visible_area)
  { // visible area — render first, then set DMA to freshly rendered buffer
    if (!(y & 1))
    {
      active_buf_idx++;

#ifdef HDMI_AUDIO_ENABLE
      // VSYNC is never asserted during the active region's back porch
      hdmi_audio_build_island(v_out_dma_buf[active_buf_idx & 1] + VACTIVE_ISLAND_OFFSET, false);
#endif

      if (scr_buffer != NULL)
      {
        uint16_t scaled_y = y / video_mode.div;
        uint8_t *scr_line = &scr_buffer[scaled_y * (V_BUF_W / 2)];
        uint32_t *line_buf = v_out_dma_buf[active_buf_idx & 1] + VACTIVE_CMD_WORDS;

        for (int x = h_margin; x--;)
          *line_buf++ = pixels[0]; // left margin

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

        for (int x = h_margin; x--;)
          *line_buf++ = pixels[0]; // right margin
      }
    }

#ifdef HDMI_AUDIO_ENABLE
    else
    {
      // Doubled line: re-send the same pixels but with a fresh data island, so
      // audio samples are not transmitted twice. The previous buffer is the one
      // not in flight, so copy the rendered pixels into it and swap.
      uint32_t *prev = v_out_dma_buf[active_buf_idx & 1];
      active_buf_idx++;
      uint32_t *next = v_out_dma_buf[active_buf_idx & 1];

      hdmi_copy_words(next + VACTIVE_CMD_WORDS, prev + VACTIVE_CMD_WORDS, v_out_dma_buf_words);
      hdmi_audio_build_island(next + VACTIVE_ISLAND_OFFSET, false);
    }
#endif

    ch->read_addr = (uintptr_t)v_out_dma_buf[active_buf_idx & 1];
    ch->transfer_count = v_out_dma_line_words;
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  {
#ifdef HDMI_AUDIO_ENABLE
    vblank_on_toggle++;
    uint32_t slot = vblank_on_toggle & 1;
    hdmi_audio_build_island(vblank_line_vsync_on[slot] + VBLANK_ISLAND_OFFSET, true);
    ch->read_addr = (uintptr_t)vblank_line_vsync_on[slot];
    ch->transfer_count = VBLANK_LINE_WORDS;
#else
    ch->read_addr = (uintptr_t)vblank_line_vsync_on;
    ch->transfer_count = count_of(vblank_line_vsync_on);
#endif
  }
  else
  {
#ifdef HDMI_AUDIO_ENABLE
    vblank_off_toggle++;
    uint32_t slot = vblank_off_toggle & 1;
    hdmi_audio_build_island(vblank_line_vsync_off[slot] + VBLANK_ISLAND_OFFSET, false);
    ch->read_addr = (uintptr_t)vblank_line_vsync_off[slot];
    ch->transfer_count = VBLANK_LINE_WORDS;
#else
    ch->read_addr = (uintptr_t)vblank_line_vsync_off;
    ch->transfer_count = count_of(vblank_line_vsync_off);
#endif
  }
}

void start_dvi()
{
  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // hstx_div=2 for 640×480@60 and 720×576@50; hstx_div=1 for 800×600@72
  // clock_configure_int_divider used for all cases — clock_configure_undivided
  // behaves differently on RP2350 and causes instability at 800×600@72Hz
  uint32_t hstx_div = video_mode.sys_freq == video_modes[MODE_800x600_72Hz]->sys_freq ? 1 : 2;
  clock_configure_int_divider(clk_hstx, 0, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys), hstx_div);

  // === Color lookup table ===
  // RGBI[3:0]: bit 3=I, 2=R, 1=G, 0=B; intensity: 1→255, 0→170
  // pixels[]: byte (2 packed RGBI nibbles) → 4 RGB332 pixels (each pixel doubled)
  uint8_t rgb332[16];

  for (int c = 0; c < 16; c++)
  {
    uint8_t Y = (c >> 3) & 1;
    uint8_t R = ((c >> 2) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t G = ((c >> 1) & 1) ? (Y ? 255 : 170) : 0;
    uint8_t B = ((c >> 0) & 1) ? (Y ? 255 : 170) : 0;
    rgb332[c] = (R & 0xe0) | ((G >> 3) & 0x1c) | ((B >> 6) & 0x03);
  }

  for (int i = 0; i < 256; i++)
  {
    uint8_t c1 = rgb332[i & 0x0f];
    uint8_t c2 = rgb332[i >> 4];
    pixels[i] = (uint32_t)c1 | ((uint32_t)c1 << 8) | ((uint32_t)c2 << 16) | ((uint32_t)c2 << 24);
  }

  // === HSTX encoder ===
  // RGB332 lane bit extraction: N-1 bits each, rotated to MSB
  hstx_ctrl_hw->expand_tmds =
      2 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // Red: 3 bits
      0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |   // Red already at MSBs [7:5]
      2 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // Green: 3 bits
      29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |  // Rotate [4:2] → MSBs
      1 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // Blue: 2 bits
      26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;   // Rotate [1:0] → MSBs

  // 4 pixels per 32-bit word for TMDS; 1 word per raw (sync) word
  hstx_ctrl_hw->expand_shift =
      4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
      8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
      1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
      0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

  // Serializer is enabled just before the DMA starts (see the end of start_dvi), so the sink
  // never sees a TMDS clock without data while the buffers are being prepared.

  // === HSTX pin mapping ===
  // HSTX outputs 0–7 → GPIO 12–19; CLK on GPIO 12-13, data on GPIO 14-19
  uint clk_bit = DVI_PIN_CLK0 - 12;
  hstx_ctrl_hw->bit[clk_bit] = HSTX_CTRL_BIT0_CLK_BITS;
  hstx_ctrl_hw->bit[clk_bit + 1] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;

  // TMDS lanes: 0=Blue, 1=Green, 2=Red
#ifdef DVI_PINS_REVERSED
  // Reversed: B→GPIO14, G→GPIO16, R→GPIO18
  static const int lane_to_output_bit[3] = {
      DVI_PIN_D0 - 12,
      DVI_PIN_D0 - 12 + 2,
      DVI_PIN_D0 - 12 + 4};
#else
  // Normal: R→GPIO14, G→GPIO16, B→GPIO18
  static const int lane_to_output_bit[3] = {
      DVI_PIN_D0 - 12 + 4,
      DVI_PIN_D0 - 12 + 2,
      DVI_PIN_D0 - 12};
#endif

  for (uint lane = 0; lane < 3; ++lane)
  {
    int bit = lane_to_output_bit[lane];
    uint32_t sel = (lane * 10) << HSTX_CTRL_BIT0_SEL_P_LSB |
                   (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
#ifdef DVI_PINS_REVERSED
    // Reversed: first pin = −, second = +
    hstx_ctrl_hw->bit[bit] = sel | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[bit + 1] = sel;
#else
    hstx_ctrl_hw->bit[bit] = sel;
    hstx_ctrl_hw->bit[bit + 1] = sel | HSTX_CTRL_BIT0_INV_BITS;
#endif
  }

  // === Sync control words ===
  // sync_polarity: bit 7=HSYNC, bit 6=VSYNC; set=negative (active-low)
  // Blue channel: D0=HSYNC, D1=VSYNC; ctrl[vsync_val][hsync_val]
  bool h_neg = video_mode.sync_polarity & 0b10000000;
  bool v_neg = video_mode.sync_polarity & 0b01000000;

  uint32_t ctrl[2][2];
  ctrl[0][0] = TMDS_CTRL_00;
  ctrl[0][1] = TMDS_CTRL_01;
  ctrl[1][0] = TMDS_CTRL_10;
  ctrl[1][1] = TMDS_CTRL_11;

  uint32_t r_g = (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20);
  SYNC_V0_H0 = ctrl[v_neg ? 0 : 1][h_neg ? 0 : 1] | r_g; // vsync active, hsync active
  SYNC_V0_H1 = ctrl[v_neg ? 0 : 1][h_neg ? 1 : 0] | r_g; // vsync active, hsync idle
  SYNC_V1_H0 = ctrl[v_neg ? 1 : 0][h_neg ? 0 : 1] | r_g; // vsync idle, hsync active
  SYNC_V1_H1 = ctrl[v_neg ? 1 : 0][h_neg ? 1 : 0] | r_g; // vsync idle, hsync idle

#ifdef HDMI_AUDIO_ENABLE
  // The data island lives inside HSYNC (preceded by 4 ctrl px + 8 preamble px);
  // the back porch must fit the 8 px video preamble + 2 px video guard band.
  hard_assert(video_mode.h_sync_pulse >= HDMI_ISLAND_SEGMENT_PIXELS);
  hard_assert(video_mode.h_back_porch >= HDMI_VIDEO_LEAD_PIXELS);
  VIDEO_GUARD_WORD = VIDEO_GUARD_CH0 | (VIDEO_GUARD_CH12 << 10) | (VIDEO_GUARD_CH12 << 20);
  hdmi_audio_init();
#endif

  // === Line buffers ===
  // Active-line: cmd prefix (VACTIVE_CMD_WORDS) + pixel data; both buffers contiguous
  v_out_dma_buf_words = h_visible_area + h_margin * 2;
  v_out_dma_line_words = VACTIVE_CMD_WORDS + v_out_dma_buf_words;

  v_out_dma_buf_alloc = calloc(v_out_dma_line_words * 2, sizeof(uint32_t));
  if (!v_out_dma_buf_alloc)
    watchdog_reboot(0, 0, 0);

  v_out_dma_buf[0] = v_out_dma_buf_alloc;
  v_out_dma_buf[1] = v_out_dma_buf_alloc + v_out_dma_line_words;

  // Fill constant HSTX command prefix (sync preamble + pixel count)
  for (int b = 0; b < 2; b++)
  {
#ifdef HDMI_AUDIO_ENABLE
    // indices [VACTIVE_ISLAND_OFFSET .. +35] (the data island) are refreshed
    // every line by hdmi_audio_build_island() in dma_handler_dvi
    int n = fill_hblank_island_prefix(v_out_dma_buf[b], SYNC_V1_H1, SYNC_V1_H0);
    v_out_dma_buf[b][n++] = HSTX_CMD_RAW_REPEAT | (video_mode.h_back_porch - HDMI_VIDEO_LEAD_PIXELS);
    v_out_dma_buf[b][n++] = SYNC_V1_H1;
    v_out_dma_buf[b][n++] = HSTX_CMD_RAW | HDMI_VIDEO_LEAD_PIXELS;
    // video preamble: CTL0=1, CTL1=CTL2=CTL3=0
    for (int i = 0; i < HDMI_PREAMBLE_PIXELS; i++)
      v_out_dma_buf[b][n++] = (SYNC_V1_H1 & 0x3ffu) | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20);
    v_out_dma_buf[b][n++] = VIDEO_GUARD_WORD;
    v_out_dma_buf[b][n++] = VIDEO_GUARD_WORD;
    v_out_dma_buf[b][n++] = HSTX_CMD_TMDS | video_mode.h_visible_area;
    // pre-seed the island itself so the very first lines don't send garbage
    hdmi_audio_build_island(v_out_dma_buf[b] + VACTIVE_ISLAND_OFFSET, false);
#else
    v_out_dma_buf[b][0] = HSTX_CMD_RAW_REPEAT | video_mode.h_front_porch;
    v_out_dma_buf[b][1] = SYNC_V1_H1;
    v_out_dma_buf[b][2] = HSTX_CMD_NOP;
    v_out_dma_buf[b][3] = HSTX_CMD_RAW_REPEAT | video_mode.h_sync_pulse;
    v_out_dma_buf[b][4] = SYNC_V1_H0;
    v_out_dma_buf[b][5] = HSTX_CMD_NOP;
    v_out_dma_buf[b][6] = HSTX_CMD_RAW_REPEAT | video_mode.h_back_porch;
    v_out_dma_buf[b][7] = SYNC_V1_H1;
    v_out_dma_buf[b][8] = HSTX_CMD_TMDS | video_mode.h_visible_area;
#endif
  }

  // === Vblank command lists ===
#ifdef HDMI_AUDIO_ENABLE
  for (int b = 0; b < 2; b++)
  {
    uint32_t *off = vblank_line_vsync_off[b];
    uint32_t *on = vblank_line_vsync_on[b];

    // no vsync
    int n = fill_hblank_island_prefix(off, SYNC_V1_H1, SYNC_V1_H0);
    off[n++] = HSTX_CMD_RAW_REPEAT | (video_mode.h_back_porch + video_mode.h_visible_area);
    off[n++] = SYNC_V1_H1;
    off[n++] = HSTX_CMD_NOP;
    hdmi_audio_build_island(off + VBLANK_ISLAND_OFFSET, false);
    // with vsync
    n = fill_hblank_island_prefix(on, SYNC_V0_H1, SYNC_V0_H0);
    on[n++] = HSTX_CMD_RAW_REPEAT | (video_mode.h_back_porch + video_mode.h_visible_area);
    on[n++] = SYNC_V0_H1;
    on[n++] = HSTX_CMD_NOP;
    hdmi_audio_build_island(on + VBLANK_ISLAND_OFFSET, true);
  }
#else
  // no vsync
  vblank_line_vsync_off[0] = HSTX_CMD_RAW_REPEAT | video_mode.h_front_porch;
  vblank_line_vsync_off[1] = SYNC_V1_H1;
  vblank_line_vsync_off[2] = HSTX_CMD_RAW_REPEAT | video_mode.h_sync_pulse;
  vblank_line_vsync_off[3] = SYNC_V1_H0;
  vblank_line_vsync_off[4] = HSTX_CMD_RAW_REPEAT | (video_mode.h_back_porch + video_mode.h_visible_area);
  vblank_line_vsync_off[5] = SYNC_V1_H1;
  vblank_line_vsync_off[6] = HSTX_CMD_NOP;
  vblank_line_vsync_off[7] = HSTX_CMD_NOP;
  // with vsync
  vblank_line_vsync_on[0] = HSTX_CMD_RAW_REPEAT | video_mode.h_front_porch;
  vblank_line_vsync_on[1] = SYNC_V0_H1;
  vblank_line_vsync_on[2] = HSTX_CMD_RAW_REPEAT | video_mode.h_sync_pulse;
  vblank_line_vsync_on[3] = SYNC_V0_H0;
  vblank_line_vsync_on[4] = HSTX_CMD_RAW_REPEAT | (video_mode.h_back_porch + video_mode.h_visible_area);
  vblank_line_vsync_on[5] = SYNC_V0_H1;
  vblank_line_vsync_on[6] = HSTX_CMD_NOP;
  vblank_line_vsync_on[7] = HSTX_CMD_NOP;
#endif

  // === DMA initialization ===
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);

  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);
  channel_config_set_dreq(&c0, DREQ_HSTX);
  channel_config_set_chain_to(&c0, dma_ch1);

  dma_channel_configure(
      dma_ch0,
      &c0,
      &hstx_fifo_hw->fifo,
#ifdef HDMI_AUDIO_ENABLE
      vblank_line_vsync_off[0],
      VBLANK_LINE_WORDS,
#else
      vblank_line_vsync_off,
      count_of(vblank_line_vsync_off),
#endif
      false //
  );

  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);
  channel_config_set_dreq(&c1, DREQ_HSTX);
  channel_config_set_chain_to(&c1, dma_ch0);

  dma_channel_configure(
      dma_ch1,
      &c1,
      &hstx_fifo_hw->fifo,
#ifdef HDMI_AUDIO_ENABLE
      vblank_line_vsync_off[0],
      VBLANK_LINE_WORDS,
#else
      vblank_line_vsync_off,
      count_of(vblank_line_vsync_off),
#endif
      false //
  );

  dma_hw->ints0 = (1u << dma_ch0) | (1u << dma_ch1);
  dma_hw->inte0 = (1u << dma_ch0) | (1u << dma_ch1);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_dvi);
  irq_set_priority(DMA_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
  irq_set_enabled(DMA_IRQ_0, true);

  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

  // Serializer: CLKDIV=5, N_SHIFTS=5, SHIFT=2 → 10 TMDS bits per pixel clock
  hstx_ctrl_hw->csr = 0;
  hstx_ctrl_hw->csr =
      HSTX_CTRL_CSR_EXPAND_EN_BITS |
      5u << HSTX_CTRL_CSR_CLKDIV_LSB |
      5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
      2u << HSTX_CTRL_CSR_SHIFT_LSB |
      HSTX_CTRL_CSR_EN_BITS;

  // set DVI data pins
  for (int i = DVI_PIN_D0; i < DVI_PIN_D0 + 6; i++)
  {
    gpio_set_function(i, GPIO_FUNC_HSTX);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // set DVI clock pins
  for (int i = DVI_PIN_CLK0; i < DVI_PIN_CLK0 + 2; i++)
  {
    gpio_set_function(i, GPIO_FUNC_HSTX);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  dma_channel_start(dma_ch0);
}

void stop_dvi()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_0, false);
  irq_remove_handler(DMA_IRQ_0, dma_handler_dvi);

  // reset ISR state for clean restart
  y = 1;
  scr_buffer = NULL;
  active_buf_idx = 0;

#ifdef HDMI_AUDIO_ENABLE
  hdmi_audio_deinit();
  vblank_off_toggle = 0;
  vblank_on_toggle = 0;
#endif

  // cleanup and free all DMA channels
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);
  // Drain HSTX FIFO before disabling — stale data would corrupt the first frame
  // after restart and prevent monitor sync
  while (!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_EMPTY_BITS))
    tight_loop_contents();

  // Disable HSTX
  hstx_ctrl_hw->csr = 0;

  // Release GPIO pins
  for (int i = 12; i <= 19; ++i)
    gpio_set_function(i, GPIO_FUNC_NULL);

  // free index buffers (single contiguous allocation)
  if (v_out_dma_buf_alloc != NULL)
  {
    free(v_out_dma_buf_alloc);
    v_out_dma_buf_alloc = NULL;
  }

  v_out_dma_buf[0] = NULL;
  v_out_dma_buf[1] = NULL;
}