#pragma once

#define CAP_LINE_LENGTH 1024
// the number of DMA buffers can be increased if there is image fluttering
#define CAP_DMA_BUF_CNT 8
#define CAP_DMA_BUF_SIZE (CAP_LINE_LENGTH * CAP_DMA_BUF_CNT)

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