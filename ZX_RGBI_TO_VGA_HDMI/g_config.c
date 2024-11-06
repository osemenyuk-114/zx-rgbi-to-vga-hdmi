#include "g_config.h"

video_mode_t vga_640x480 = {
    .sys_freq = 252000,
    .pixel_freq = 25175000.0,
    .h_visible_area = 640,
    .v_visible_area = 480,
    .whole_line = 800,
    .whole_frame = 525,
    .h_front_porch = 16,
    .h_sync_pulse = 96,
    .h_back_porch = 48,
    .v_front_porch = 10,
    .v_sync_pulse = 2,
    .v_back_porch = 33,
    .sync_polality = 0b11000000, // negative
    .div = 2,
};

video_mode_t vga_800x600 = {
    .sys_freq = 240000,
    .pixel_freq = 40000000.0,
    .h_visible_area = 800,
    .v_visible_area = 600,
    .whole_line = 1056,
    .whole_frame = 628,
    .h_front_porch = 40,
    .h_sync_pulse = 128,
    .h_back_porch = 88,
    .v_front_porch = 1,
    .v_sync_pulse = 4,
    .v_back_porch = 23,
    .sync_polality = 0b00000000, // positive
    .div = 2,
};

video_mode_t vga_1024x768 = {
    .sys_freq = 260000,
    .pixel_freq = 65000000.0,
    .h_visible_area = 1023, // 1024
    .v_visible_area = 768,
    .whole_line = 1344,
    .whole_frame = 806,
    .h_front_porch = 24,
    .h_sync_pulse = 135, // 136
    .h_back_porch = 162, // 160
    .v_front_porch = 3,
    .v_sync_pulse = 6,
    .v_back_porch = 29,
    .sync_polality = 0b11000000, // negative
    .div = 3,
};

video_mode_t vga_1280x1024_d3 = {
    .sys_freq = 252000,
    .pixel_freq = 108000000.0,
    .h_visible_area = 1278, // 1280
    .v_visible_area = 1024,
    .whole_line = 1692, // 1688
    .whole_frame = 1066,
    .h_front_porch = 48,
    .h_sync_pulse = 111, // 112
    .h_back_porch = 255, // 248
    .v_front_porch = 1,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polality = 0b00000000, // positive
    .div = 3,
};

video_mode_t vga_1280x1024_d4 = {
    .sys_freq = 252000, // 243000, // when scanline mode is on, a black line randomly appears on the screen for a short moment
    .pixel_freq = 108000000.0,
    .h_visible_area = 1280,
    .v_visible_area = 1024,
    .whole_line = 1680, // 1688
    .whole_frame = 1066,
    .h_front_porch = 48,
    .h_sync_pulse = 112,
    .h_back_porch = 240, // 248
    .v_front_porch = 1,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polality = 0b00000000, // positive
    .div = 4,
};

video_mode_t *vga_modes[] = {&vga_640x480, &vga_640x480, &vga_800x600, &vga_1024x768, &vga_1280x1024_d3, &vga_1280x1024_d4};

uint8_t g_gbuf[V_BUF_SZ * 3];
