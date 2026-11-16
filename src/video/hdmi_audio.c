#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "g_config.h"
#include "hdmi_audio.h"
#include "hdmi_audio.pio.h"

#ifdef HDMI_AUDIO_ENABLE

extern video_mode_t video_mode;
extern settings_t settings;

// 512 stereo pairs (~10.7 ms at 48 kHz) absorb the jitter between the PCM1808 clock and the video ISR
#define I2S_RING_WORDS 1024
#define I2S_RING_BYTES (I2S_RING_WORDS * sizeof(uint32_t))
#define I2S_RING_BITS 12 // log2(I2S_RING_BYTES)

// Data-island guard band: channels 1 & 2 (green, red) use a fixed pattern
#define DI_GUARD_CH12 0b0100110011u

// Recommended N for 48 kHz (Table 7-2): CTS = pixel_clk / 1000, an integer for all project pixel clocks
#define ACR_N 6144u

#define IEC60958_STATUS_BITS 192
#define SAMPLING_FREQUENCY_48K 0b0010 // IEC 60958-3 Table 2

#define RATE_OVERFLOW_PAIRS 384 // I2S ring nearly full: drop the oldest samples

// ACR must be sent at least every ~21 ms and no more often than ~300 us (HDMI 1.4b 7.2)
#define ACR_PERIOD 32
#define ISLAND_RING_SLOTS 128

// Sample-packet pacing: Q32 credit per video line, one packet (4 pairs) per credit, 12000 packets/s at 48 kHz
#define PACE_ONE (1ull << 32)
#define PACE_ARM_LEVEL 48 // ~4 ms of buffered packets before releasing them

// I2S receiver: PIO bit capture + ring-buffer DMA

static_assert((1u << I2S_RING_BITS) == I2S_RING_BYTES, "I2S_RING_BITS must match I2S_RING_BYTES");

static uint32_t i2s_ring_buf[I2S_RING_WORDS] __attribute__((aligned(I2S_RING_BYTES)));
static int i2s_dma_ch = -1;
static uint i2s_pio_offset;
static uint32_t i2s_read_idx; // consumer index (words), free-running
static uint64_t popped_pairs; // input pairs consumed from the I2S ring
static bool i2s_started;

// Returns the DMA write pointer translated to a word index within the ring.
static inline __attribute__((always_inline)) uint32_t i2s_write_idx()
{
  uintptr_t waddr = (uintptr_t)dma_hw->ch[i2s_dma_ch].write_addr;
  return (uint32_t)((waddr - (uintptr_t)i2s_ring_buf) / sizeof(uint32_t));
}

static inline __attribute__((always_inline)) uint32_t i2s_pairs_available()
{
  uint32_t avail_words = (i2s_write_idx() - i2s_read_idx) & (I2S_RING_WORDS - 1);
  return avail_words / 2;
}

// Pops one Left/Right sample pair, sign-extended to 32 bits (24 significant
// bits). Caller must ensure i2s_pairs_available() >= 1 first.
static inline __attribute__((always_inline)) void i2s_pop_pair(int32_t *left, int32_t *right)
{
  uint32_t l = i2s_ring_buf[i2s_read_idx & (I2S_RING_WORDS - 1)];
  uint32_t r = i2s_ring_buf[(i2s_read_idx + 1) & (I2S_RING_WORDS - 1)];
  i2s_read_idx += 2;
  popped_pairs++;

  // top 24 bits are the MSB-first sample; bottom 8 bits are I2S padding
  *left = (int32_t)l >> 8;
  *right = (int32_t)r >> 8;
}

static void i2s_pio_start()
{
  static_assert(HDMI_AUDIO_PIN_BCK == HDMI_AUDIO_PIN_DATA + 1 && HDMI_AUDIO_PIN_LRCK == HDMI_AUDIO_PIN_DATA + 2,
                "pio_i2s_rx addresses BCK/LRCK as relative pins 1/2 of IN_BASE=DATA; "
                "DATA, BCK, LRCK must be 3 consecutive GPIOs in that order");

  if (!i2s_started)
  {
    i2s_pio_offset = pio_add_program(PIO_AUDIO, &pio_i2s_rx_program);

    pio_gpio_init(PIO_AUDIO, HDMI_AUDIO_PIN_DATA);
    pio_gpio_init(PIO_AUDIO, HDMI_AUDIO_PIN_BCK);
    pio_gpio_init(PIO_AUDIO, HDMI_AUDIO_PIN_LRCK);
    gpio_set_input_hysteresis_enabled(HDMI_AUDIO_PIN_DATA, true);
    gpio_set_input_hysteresis_enabled(HDMI_AUDIO_PIN_BCK, true);
    gpio_set_input_hysteresis_enabled(HDMI_AUDIO_PIN_LRCK, true);
    i2s_started = true;
  }

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, i2s_pio_offset + pio_i2s_rx_wrap_target, i2s_pio_offset + pio_i2s_rx_wrap);
  sm_config_set_in_pins(&c, HDMI_AUDIO_PIN_DATA); // IN_BASE: relative pin 0=DATA, 1=BCK, 2=LRCK
  sm_config_set_jmp_pin(&c, HDMI_AUDIO_PIN_LRCK);
  sm_config_set_in_shift(&c, false, true, 32); // shift left: first (MSB) bit ends up in bit 31; autopush at 32 bits
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv_int_frac(&c, 1, 0); // unthrottled — paced entirely by external BCK/LRCK

  pio_sm_init(PIO_AUDIO, SM_AUDIO, i2s_pio_offset + pio_i2s_rx_offset_start, &c);
  pio_sm_set_enabled(PIO_AUDIO, SM_AUDIO, true);
}

static void i2s_dma_start()
{
  memset(i2s_ring_buf, 0, sizeof(i2s_ring_buf));
  i2s_read_idx = 0;
  popped_pairs = 0;

  i2s_dma_ch = dma_claim_unused_channel(true);

  dma_channel_config c = dma_channel_get_default_config(i2s_dma_ch);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, false);
  channel_config_set_write_increment(&c, true);
  channel_config_set_dreq(&c, DREQ_PIO_AUDIO + SM_AUDIO);
  channel_config_set_ring(&c, true, I2S_RING_BITS); // wrap write_addr within the ring

  // endless transfer count (RP2350) + ring-wrapped write address gives perpetual capture;
  // a finite count would stop after ~12.4 h and chaining a channel to itself disables the trigger
  dma_channel_configure(
      i2s_dma_ch,
      &c,
      i2s_ring_buf,
      &PIO_AUDIO->rxf[SM_AUDIO],
      dma_encode_endless_transfer_count(),
      true // start immediately
  );
}

// TERC4 / data-island encoding (HDMI 1.4a 5.2.3, 5.4.3)

// TERC4 4-to-10 bit table (Table 5-4)
static const uint16_t __not_in_flash("hdmi_audio_tables") terc4_table[16] = {
    0b1010011100, 0b1001100011, 0b1011100100, 0b1011100010,
    0b0101110001, 0b0100011110, 0b0110001110, 0b0100111100,
    0b1011001100, 0b0100111001, 0b0110011100, 0b1011000110,
    0b1010001110, 0b1001110001, 0b0101100011, 0b1011000011};

// Data-island guard band channel 0 (blue) depends on the (vsync,hsync) wire levels (HDMI 1.4a 5.2.3.3)
static const uint16_t __not_in_flash("hdmi_audio_tables") di_guard_ch0[2][2] = {
    // [vsync_wire][hsync_wire]
    {0b1010001110, 0b1001110001},
    {0b0101100011, 0b1011000011},
};

// Packs 3 already-TMDS-coded 10-bit channel symbols into one 30-bit HSTX
// raw word, matching the channel layout used for the sync words in
// dvi_hstx.c (ch0=blue=bits[9:0], ch1=green=bits[19:10], ch2=red=bits[29:20]).
static inline uint32_t pack_tmds3(uint16_t ch0, uint16_t ch1, uint16_t ch2)
{
  return (uint32_t)ch0 | ((uint32_t)ch1 << 10) | ((uint32_t)ch2 << 20);
}

static uint8_t ecc_table[256];
static uint32_t terc4_ch1[16], terc4_ch2[16];

static void encoding_tables_init()
{
  for (uint32_t i = 0; i < 256; i++)
  {
    uint8_t ecc = i;
    for (int bit = 0; bit < 8; bit++)
      ecc = (ecc >> 1) ^ ((ecc & 1) ? 0x83 : 0);
    ecc_table[i] = ecc;
  }

  for (int i = 0; i < 16; i++)
  {
    terc4_ch1[i] = (uint32_t)terc4_table[i] << 10;
    terc4_ch2[i] = (uint32_t)terc4_table[i] << 20;
  }
}

// BCH with polynomial 0x83, LSB-first, one byte per lookup.
static inline __attribute__((always_inline)) uint8_t compute_ecc(uint64_t value, uint8_t bytes)
{
  uint8_t ecc = 0;

  for (uint8_t i = 0; i < bytes; i++, value >>= 8)
    ecc = ecc_table[ecc ^ (uint8_t)value];

  return ecc;
}

// Encodes one full 32-pixel data island packet (header + 4 subpackets) into
// `dst`, bracketed by its leading/trailing guard bands (HDMI_AUDIO_ISLAND_WORDS
// words total). `header` uses its low 24 bits; `sub[4]` use their low 56 bits.
static void __not_in_flash_func(encode_island)(uint32_t *dst, uint32_t header, const uint64_t sub[4], bool vsync_active)
{
  bool v_neg = (video_mode.sync_polarity & 0b01000000) != 0;
  bool h_neg = (video_mode.sync_polarity & 0b10000000) != 0;
  uint8_t vwire = vsync_active ? (v_neg ? 0 : 1) : (v_neg ? 1 : 0);
  uint8_t hwire = h_neg ? 0 : 1; // islands are placed inside the HSYNC pulse, so HSYNC is active

  uint16_t guard_ch0 = di_guard_ch0[vwire][hwire];
  uint32_t guard_word = pack_tmds3(guard_ch0, DI_GUARD_CH12, DI_GUARD_CH12);

  dst[0] = guard_word;
  dst[1] = guard_word;

  uint32_t header_bits = (header & 0xffffffu) | ((uint32_t)compute_ecc(header, 3) << 24);
  uint32_t low[4], high[4];

  for (int k = 0; k < 4; k++)
  {
    low[k] = (uint32_t)sub[k];
    high[k] = ((uint32_t)(sub[k] >> 32) & 0xffffffu) | ((uint32_t)compute_ecc(sub[k], 7) << 24);
  }

  // Two 32-bit halves avoid variable 64-bit shifts in the pixel loop.
  for (int half = 0; half < 2; half++)
  {
    uint32_t *words = half ? high : low;
    uint32_t s0 = words[0], s1 = words[1], s2 = words[2], s3 = words[3];

    for (int n = 0; n < 16; n++)
    {
      uint32_t ch0_nibble = ((half || n) ? 8u : 0u) | ((header_bits & 1u) << 2) | (vwire << 1) | hwire;
      uint32_t ch1_nibble = (s0 & 1u) | ((s1 & 1u) << 1) | ((s2 & 1u) << 2) | ((s3 & 1u) << 3);
      uint32_t ch2_nibble = ((s0 >> 1) & 1u) | (s1 & 2u) | ((s2 & 2u) << 1) | ((s3 & 2u) << 2);

      dst[2 + half * 16 + n] = terc4_table[ch0_nibble] | terc4_ch1[ch1_nibble] | terc4_ch2[ch2_nibble];
      header_bits >>= 1;
      s0 >>= 2;
      s1 >>= 2;
      s2 >>= 2;
      s3 >>= 2;
    }
  }

  dst[34] = guard_word;
  dst[35] = guard_word;
}

// Audio Clock Regeneration (ACR) packet (HDMI 1.4a 5.3.3)

// CTS = pixel_clk * 48 / measured fs, Q16; the fraction is dithered between neighbouring integers per packet
static volatile uint32_t cts_q16;
static uint32_t cts_frac_acc;

static void __not_in_flash_func(build_acr_packet)(uint32_t *header, uint64_t sub[4])
{
  uint32_t q = cts_q16;
  cts_frac_acc += q & 0xffff;
  uint32_t cts = (q >> 16) + (cts_frac_acc >> 16);
  cts_frac_acc &= 0xffff;

  // "An HDMI Sink shall ignore bytes HB1 and HB2 of the ACR packet header."
  *header = 0x01;

  // Subpacket byte layout (HDMI 1.4a Table 5-11, MSB-first): N[7:0],
  // N[15:8], {4'b0,N[19:16]}, CTS[7:0], CTS[15:8], {4'b0,CTS[19:16]}, reserved.
  uint64_t s = ((uint64_t)(ACR_N & 0xff) << 48) |
               ((uint64_t)((ACR_N >> 8) & 0xff) << 40) |
               ((uint64_t)((ACR_N >> 16) & 0x0f) << 32) |
               ((uint64_t)(cts & 0xff) << 24) |
               ((uint64_t)((cts >> 8) & 0xff) << 16) |
               ((uint64_t)((cts >> 16) & 0x0f) << 8);
  // bits [7:0] (transmitted last) are reserved/zero

  for (int k = 0; k < 4; k++)
    sub[k] = s;
}

// InfoFrames: AVI (0x82) and Audio (0x84) (HDMI 1.4a 8.2)

static void build_infoframe(uint32_t *header, uint64_t sub[4], uint8_t type, uint8_t version, uint8_t length, const uint8_t *pb)
{
  uint8_t body[28] = {0};

  memcpy(&body[1], pb, length);

  uint8_t sum = type + version + length;

  for (int i = 1; i <= length; i++)
    sum += body[i];

  body[0] = (uint8_t)(0x100 - sum); // checksum: all bytes sum to 0 (mod 256)

  *header = type | (version << 8) | (length << 16);

  for (int k = 0; k < 4; k++)
  {
    uint64_t s = 0;

    for (int b = 0; b < 7; b++)
      s |= (uint64_t)body[k * 7 + b] << (8 * b);

    sub[k] = s;
  }
}

static void build_avi_packet(uint32_t *header, uint64_t sub[4])
{
  uint8_t pb[13] = {0};
  uint8_t vic = 0;

  if (video_mode.h_visible_area == 640 && video_mode.v_visible_area == 480)
    vic = 1;
  else if (video_mode.h_visible_area == 720 && video_mode.v_visible_area == 576)
    vic = 17;

  pb[0] = 0x00; // PB1: RGB, no bar/scan info
  pb[1] = 0x18; // PB2: 4:3 picture, active format same as picture
  pb[2] = 0x08; // PB3: full-range RGB
  pb[3] = vic;  // PB4: video identification code (0 = none)
  build_infoframe(header, sub, 0x82, 2, 13, pb);
}

static void build_audio_infoframe_packet(uint32_t *header, uint64_t sub[4])
{
  uint8_t pb[10] = {0};

  pb[0] = 0x01; // PB1: 2 channels, coding type/sample size/frequency from stream
  build_infoframe(header, sub, 0x84, 1, 10, pb);
}

// Audio Sample Packet (ASP) (HDMI 1.4a 5.3.4, IEC 60958-3)

static uint8_t channel_status[2][24]; // [0]=left, [1]=right
static uint32_t frame_counter = 0;    // position within the 192-bit channel status cyclic stream

static void channel_status_init()
{
  for (int ch = 0; ch < 2; ch++)
  {
    memset(channel_status[ch], 0, sizeof(channel_status[ch]));
    // byte0: mode(2)<<6 | pre-emphasis(3)<<3 | copyright_not_asserted(1)<<2 | non-PCM(1)<<1 | professional(1)
    channel_status[ch][0] = 0x04;
    // byte1: category code (general)
    channel_status[ch][1] = 0x00;
    // byte2: channel number(4)<<4 | source number(4); left=1, right=2
    channel_status[ch][2] = (uint8_t)((ch + 1) << 4);
    // byte3: clock accuracy(2)<<4 | sampling frequency(4)
    channel_status[ch][3] = SAMPLING_FREQUENCY_48K;
    // byte4: original sampling frequency(4)<<4 | word length(4) — not indicated
    channel_status[ch][4] = 0x00;
    // bytes 5..23 reserved, left zero
  }
}

static inline __attribute__((always_inline)) uint8_t channel_status_bit(int ch, uint32_t pos)
{
  return (channel_status[ch][pos >> 3] >> (pos & 7)) & 1;
}

// The I2S source has its own free-running clock, so its real rate differs from 48 kHz by tens of ppm.
// The rate is measured over a sliding 8..16 s window and drives both ACR CTS and sample-packet pacing.
static uint64_t meas_t0_us, meas_base;
static uint64_t meas_snap_us, meas_snap_base;
static uint32_t meas_last_apply_ms;
static volatile bool audio_enabled = true;
static volatile uint8_t audio_mode = AUDIO_STEREO;

void hdmi_audio_set_enabled(bool enabled)
{
  audio_enabled = enabled;
}

void hdmi_audio_set_mode(uint8_t mode)
{
  audio_mode = mode;
}

static inline __attribute__((always_inline)) uint8_t sample_parity(uint32_t sample)
{
  sample ^= sample >> 16;
  sample ^= sample >> 8;
  sample ^= sample >> 4;
  return (0x6996u >> (sample & 0xfu)) & 1u;
}

// Builds one ASP from 4 pending L/R pairs; returns 0 if fewer than 4 are available.
static int __not_in_flash_func(build_asp_packet)(uint32_t *header, uint64_t sub[4])
{
  int n = i2s_pairs_available();

  if (n < 4)
    return 0; // batch full 4-sample packets to keep the island rate (and CPU cost) low

  if (n > RATE_OVERFLOW_PAIRS)
  {
    uint32_t dropped = n - RATE_OVERFLOW_PAIRS;
    i2s_read_idx += 2 * dropped;
    popped_pairs += dropped;
  }

  n = 4;
  *header = 0x02; // HB0 = packet type 0x02 (Audio Sample Packet), HB1=HB2=0 (2-channel layout)

  uint32_t present_mask = 0;
  uint32_t fresh_mask = 0;

  for (int i = 0; i < n; i++)
  {
    int32_t left, right;
    i2s_pop_pair(&left, &right);

    if (!audio_enabled)
      left = right = 0;
    else
    {
      int32_t l = left;
      int32_t r = right;

      switch (audio_mode)
      {
      case AUDIO_MONO_L:
        right = l;
        break;

      case AUDIO_MONO_R:
        left = r;
        break;

      case AUDIO_SWAPPED:
        left = r;
        right = l;
        break;

      default:
        break;
      }
    }

    uint32_t aligned_fc = (frame_counter + i) % IEC60958_STATUS_BITS;
    uint8_t csL = channel_status_bit(0, aligned_fc);
    uint8_t csR = channel_status_bit(1, aligned_fc);

    uint32_t sampleL = (uint32_t)left & 0xffffffu;
    uint32_t sampleR = (uint32_t)right & 0xffffffu;

    // parity (IEC 60958-1 4.5): even parity over {valid(0), user(0), channel
    // status bit, 24 sample bits}
    uint8_t parityL = csL ^ sample_parity(sampleL);
    uint8_t parityR = csR ^ sample_parity(sampleR);

    // bit0=validL,bit1=userL,bit2=csL,bit3=parityL,bit4=validR,bit5=userR,bit6=csR,bit7=parityR
    // (validity bit MUST be 0 — most sinks mute audio if V=1)
    uint8_t flags8 = (uint8_t)((csL << 2) | (parityL << 3) | (csR << 6) | (parityR << 7));

    sub[i] = ((uint64_t)flags8 << 48) | ((uint64_t)sampleR << 24) | (uint64_t)sampleL;
    present_mask |= 1u << i;

    if (aligned_fc == 0)
      fresh_mask |= 1u << i;
  }

  for (int i = n; i < 4; i++)
    sub[i] = 0;

  // header[11:8] = subpacket-present flags, header[23:20] = channel-status
  // restart ("B") flags, header[19:12] = layout (0 = 2-channel stereo)
  *header |= (present_mask & 0x0f) << 8;
  *header |= (fresh_mask & 0x0f) << 20;

  frame_counter = (frame_counter + (uint32_t)n) % IEC60958_STATUS_BITS;

  return n;
}

// Packet scheduler
// hdmi_audio_poll() pre-builds islands into a ring; the line ISR only copies them.

static uint64_t pace_acc;
static uint64_t pace_step; // credit per video line
static bool pace_armed;
static uint8_t island_kind_asp[ISLAND_RING_SLOTS]; // 1 = sample packet, 0 = control packet
static uint32_t island_ring[ISLAND_RING_SLOTS][HDMI_AUDIO_ISLAND_WORDS];
static volatile uint32_t island_ring_head = 0; // producer (poll) owns this
static volatile uint32_t island_ring_tail = 0; // consumer (ISR) owns this
static uint32_t silent_island[HDMI_AUDIO_ISLAND_WORDS];
static uint32_t avi_island[HDMI_AUDIO_ISLAND_WORDS];
static uint32_t audio_info_island[HDMI_AUDIO_ISLAND_WORDS];
static uint32_t island_counter = 0;

// Precomputed "nothing new to send" islands (idle / vsync-active), built
// once at init. Used as a zero-cost fallback whenever the ring is empty —
// the ISR never has to fall back to computing one on the spot.
static uint32_t null_island_idle[HDMI_AUDIO_ISLAND_WORDS];
static uint32_t null_island_vsync[HDMI_AUDIO_ISLAND_WORDS];

static inline __attribute__((always_inline)) bool next_island_is_asp()
{
  uint32_t slot = island_counter % ACR_PERIOD;
  return slot != 0 && !((slot == 8 || slot == 16) && ((island_counter / ACR_PERIOD) % 4) == 0);
}

static bool __not_in_flash_func(build_next_island)(uint32_t *dst)
{
  uint32_t header;
  uint64_t sub[4];
  uint32_t slot = island_counter % ACR_PERIOD;

  if (slot == 8 && ((island_counter / ACR_PERIOD) % 4) == 0)
    hdmi_copy_words(dst, avi_island, HDMI_AUDIO_ISLAND_WORDS);
  else if (slot == 16 && ((island_counter / ACR_PERIOD) % 4) == 0)
    hdmi_copy_words(dst, audio_info_island, HDMI_AUDIO_ISLAND_WORDS);
  else
  {
    if (slot == 0)
      build_acr_packet(&header, sub);
    else if (!build_asp_packet(&header, sub))
      return false;
    encode_island(dst, header, sub, false);
  }

  island_counter++;
  return true;
}

void hdmi_audio_init()
{
  encoding_tables_init();
  channel_status_init();
  audio_enabled = settings.audio_enabled;
  audio_mode = settings.audio_mode;
  frame_counter = 0;
  island_counter = 0;
  island_ring_head = 0;
  island_ring_tail = 0;
  meas_t0_us = 0;
  meas_snap_us = 0;
  meas_last_apply_ms = 0;
  cts_frac_acc = 0;
  cts_q16 = (uint32_t)((double)video_mode.pixel_freq * 48.0 / 48000.0 * 65536.0 + 0.5);

  uint64_t null_sub[4] = {0, 0, 0, 0};
  encode_island(null_island_idle, 0, null_sub, false);
  encode_island(null_island_vsync, 0, null_sub, true);

  uint32_t header;
  uint64_t sub[4];
  build_avi_packet(&header, sub);
  encode_island(avi_island, header, sub, false);
  build_audio_infoframe_packet(&header, sub);
  encode_island(audio_info_island, header, sub, false);

  // Silent Audio Sample Packet: 4 zero samples, channel-status block-start
  // flag never set (phase 4..7 of the 192-bit block) so inserting it does not
  // resynchronise the sink's channel-status decoding.
  uint64_t silent_sub[4];
  uint32_t silent_header = 0x02 | (0x0fu << 8);
  for (int i = 0; i < 4; i++)
  {
    uint8_t csL = channel_status_bit(0, 4 + i);
    uint8_t csR = channel_status_bit(1, 4 + i);
    uint8_t flags8 = (uint8_t)((csL << 2) | (csL << 3) | (csR << 6) | (csR << 7)); // zero sample: parity == cs bit
    silent_sub[i] = (uint64_t)flags8 << 48;
  }
  encode_island(silent_island, silent_header, silent_sub, false);

  pace_acc = 0;
  pace_armed = false;
  pace_step = (uint64_t)(12000.0 * 4294967296.0 * video_mode.whole_line / (double)video_mode.pixel_freq + 0.5);

  i2s_dma_start();
  i2s_pio_start();
}

void hdmi_audio_deinit()
{
  // Stop capture; the program and GPIO setup stay. The system clock may change
  // with the video mode, which can make a running PIO lose a BCK edge and stay
  // bit-shifted (noise) — a clean restart re-locks to LRCK.
  pio_sm_set_enabled(PIO_AUDIO, SM_AUDIO, false);

  if (i2s_dma_ch >= 0)
  {
    dma_channel_abort(i2s_dma_ch);
    dma_channel_unclaim(i2s_dma_ch);
    i2s_dma_ch = -1;
  }
}

static void apply_measured_rate(double fs)
{
  double pix = (double)video_mode.pixel_freq;
  uint32_t q = (uint32_t)(pix * 48.0 / fs * 65536.0 + 0.5);
  uint64_t step = (uint64_t)(fs / 4.0 * 4294967296.0 * video_mode.whole_line / pix + 0.5);

  // pace_step is read by the line ISR; 64-bit store is not atomic
  uint32_t irq = save_and_disable_interrupts();
  pace_step = step;
  cts_q16 = q;
  restore_interrupts(irq);
}

static void update_measured_rate()
{
  uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  if ((uint32_t)(now_ms - meas_last_apply_ms) < 100)
    return;
  meas_last_apply_ms = now_ms;

  uint64_t now = time_us_64();
  uint64_t total = popped_pairs + i2s_pairs_available();

  if (meas_t0_us == 0)
  {
    meas_t0_us = now;
    meas_base = total;
    meas_snap_us = 0;
    return;
  }

  uint64_t elapsed = now - meas_t0_us;
  if (elapsed < 1000000)
    return;

  double fs = (double)(total - meas_base) * 1e6 / (double)elapsed;
  if (fs < 47000.0 || fs > 49000.0)
  {
    // source stopped or restarted: re-anchor
    meas_t0_us = now;
    meas_base = total;
    meas_snap_us = 0;
    return;
  }

  apply_measured_rate(fs);

  if (!meas_snap_us && elapsed >= 8000000)
  {
    meas_snap_us = now;
    meas_snap_base = total;
  }
  else if (meas_snap_us && elapsed >= 16000000)
  {
    meas_t0_us = meas_snap_us;
    meas_base = meas_snap_base;
    meas_snap_us = 0;
  }
}

void hdmi_audio_poll()
{
  update_measured_rate();

  // Keep the ring topped up with freshly encoded islands (ACR / ASP). This
  // does the expensive TERC4/ECC work, safely off the time-critical ISR.
  while ((island_ring_head - island_ring_tail) < ISLAND_RING_SLOTS)
  {
    // Control packets are interleaved with real sample packets only; the
    // counter advances on every pushed packet, never while waiting for samples.
    bool is_asp = next_island_is_asp();
    uint32_t slot = island_ring_head % ISLAND_RING_SLOTS;
    if (!build_next_island(island_ring[slot]))
      break; // no new samples yet — leave the ring as-is for now

    island_kind_asp[slot] = is_asp;
    island_ring_head++;
  }
}

void __not_in_flash_func(hdmi_audio_build_island)(uint32_t *dst, bool vsync_active)
{
  pace_acc += pace_step;
  if (pace_acc > 4 * PACE_ONE)
    pace_acc = 4 * PACE_ONE;

  if (vsync_active)
  {
    hdmi_copy_words(dst, null_island_vsync, HDMI_AUDIO_ISLAND_WORDS);
    return;
  }

  // ISR-safe path: fixed-size copy only, no TERC4/ECC computation here.
  //
  // Called once per video line, so it doubles as the pacing clock: sample
  // packets are released at exactly 48 kHz / 4 per second (credit accumulator
  // locked to the line rate, i.e. to the TMDS clock the sink regenerates its
  // audio clock from). Control packets (ACR/InfoFrames) bypass the credit.
  // When a sample packet is due but none is available, a silent ASP is sent so
  // the sink never sees a gap in the ASP stream.
  uint32_t level = island_ring_head - island_ring_tail;

  if (level && !island_kind_asp[island_ring_tail % ISLAND_RING_SLOTS])
  {
    hdmi_copy_words(dst, island_ring[island_ring_tail % ISLAND_RING_SLOTS], HDMI_AUDIO_ISLAND_WORDS);
    island_ring_tail++;
    return;
  }

  if (pace_acc >= PACE_ONE)
  {
    pace_acc -= PACE_ONE;

    if (!pace_armed && level >= PACE_ARM_LEVEL)
      pace_armed = true;

    if (pace_armed && level)
    {
      hdmi_copy_words(dst, island_ring[island_ring_tail % ISLAND_RING_SLOTS], HDMI_AUDIO_ISLAND_WORDS);
      island_ring_tail++;
      return;
    }

    pace_armed = false; // starved: rebuild the cushion before resuming
    hdmi_copy_words(dst, silent_island, HDMI_AUDIO_ISLAND_WORDS);
    return;
  }

  hdmi_copy_words(dst, null_island_idle, HDMI_AUDIO_ISLAND_WORDS);
}

void __not_in_flash_func(hdmi_audio_build_null_island)(uint32_t *dst)
{
  hdmi_copy_words(dst, null_island_idle, HDMI_AUDIO_ISLAND_WORDS);
}

#endif // HDMI_AUDIO_ENABLE