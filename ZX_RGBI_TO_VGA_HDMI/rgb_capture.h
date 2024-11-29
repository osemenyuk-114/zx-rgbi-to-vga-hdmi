#ifndef RGB_CAPTURE_H
#define RGB_CAPTURE_H

// video timing
#define H_SYNC_PULSE (4 * 7)  //  4 µs @ 7.0 MHz pixel clock
#define V_SYNC_PULSE (30 * 7) // 30 µs @ 7.0 MHz pixel clock

#define CAP_LINE_LENGTH 1024
#define CAP_DMA_BUF_CNT 8
#define CAP_DMA_BUF_SIZE (CAP_LINE_LENGTH * CAP_DMA_BUF_CNT)

extern uint32_t frame_count;

int16_t set_capture_shX(int16_t shX);
int16_t set_capture_shY(int16_t shY);
int8_t set_capture_delay(int8_t delay);
void check_settings(settings_t *settings);
void calculate_clkdiv(float frequency, uint16_t *div_int, uint8_t *div_frac);
void start_capture(settings_t *settings);

#endif