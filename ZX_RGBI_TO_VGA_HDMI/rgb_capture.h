#ifndef RGB_CAPTURE_H
#define RGB_CAPTURE_H

#define CAP_LINE_LENGTH 1024
#define CAP_DMA_BUF_CNT 8
#define CAP_DMA_BUF_SIZE (CAP_LINE_LENGTH * CAP_DMA_BUF_CNT)

extern uint32_t frame_count;

void set_capture_frequency(uint32_t frequency);
int8_t set_ext_clk_divider(int8_t divider);
int16_t set_capture_shX(int16_t shX);
int16_t set_capture_shY(int16_t shY);
int8_t set_capture_delay(int8_t delay);
void set_pin_inversion_mask(uint8_t pin_inversion_mask);
void set_video_sync_mode(bool video_sync_mode);
void check_settings(settings_t *settings);
void start_capture(settings_t *settings);
void stop_capture();
#endif