#pragma once

typedef struct ff_osd_display_t
{
    char text[4][40];
    uint8_t cols;
    uint8_t rows;
    uint8_t heights;
    bool on;
} ff_osd_display_t;

typedef struct __attribute__((packed)) ff_osd_info_t
{
    uint8_t protocol_ver;
    uint8_t fw_major, fw_minor;
    uint8_t buttons;
} ff_osd_info_t;

extern ff_osd_display_t ff_osd_display;
extern uint8_t ff_osd_buttons_rx;
extern volatile bool ff_osd_needs_i2c_init;

void ff_osd_update();
void ff_osd_i2c_process();
void ff_osd_i2c_init();
void ff_osd_set_address();
void ff_osd_set_buttons(uint8_t buttons);
uint16_t ff_osd_set_cols(int16_t cols);
uint8_t ff_osd_set_h_position(int8_t h_position);
