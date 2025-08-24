#pragma once

#define CAP_LINE_LENGTH 1024
#define CAP_DMA_BUF_CNT 8
#define CAP_DMA_BUF_SIZE (CAP_LINE_LENGTH * CAP_DMA_BUF_CNT)

extern uint32_t frame_count;

void set_capture_frequency(uint32_t);
int8_t set_ext_clk_divider(int8_t);
int16_t set_capture_shX(int16_t);
int16_t set_capture_shY(int16_t);
int8_t set_capture_delay(int8_t);
void set_pin_inversion_mask(uint8_t);
void set_video_sync_mode(bool);
void check_settings(settings_t *);
void start_capture(settings_t *);
void stop_capture();
