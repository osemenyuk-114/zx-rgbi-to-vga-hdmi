#pragma once

extern volatile uint32_t frame_count;

void set_capture_frequency(uint32_t);
int8_t set_ext_clk_divider(int8_t);
int16_t set_capture_shX(int16_t);
int16_t set_capture_shY(int16_t);
int8_t set_capture_delay(int8_t);
void set_pin_inversion_mask(uint8_t);
void set_video_sync_mode(bool);
void start_capture();
void stop_capture();