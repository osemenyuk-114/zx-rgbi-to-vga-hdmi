#pragma once

// 32-bit HSTX words per data island: 2 guard + 32 packet + 2 guard
#define HDMI_AUDIO_ISLAND_WORDS 36

// Word copy that is always inlined, so ISR code never calls flash-resident memcpy
static inline __attribute__((always_inline)) void hdmi_copy_words(uint32_t *dst, const uint32_t *src, uint32_t words)
{
  while (words--)
    *dst++ = *src++;
}

void hdmi_audio_init();
void hdmi_audio_deinit();
// Measures the I2S rate and, in polling mode, pre-encodes islands; call often from the main loop
void hdmi_audio_poll();
// Supplies one paced island (RAM-only encode/copy); vsync_active = inside the VSYNC pulse
void hdmi_audio_build_island(uint32_t *dst, bool vsync_active);
// Copies a Null-packet island (no samples consumed)
void hdmi_audio_build_null_island(uint32_t *dst);
void hdmi_audio_set_enabled(bool enabled);
// audio_mode_t from g_config.h
void hdmi_audio_set_mode(uint8_t mode);