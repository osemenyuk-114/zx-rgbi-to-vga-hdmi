#pragma once

// Serial menu system for ZX RGB(I) to VGA/HDMI converter

// Menu print functions
void print_main_menu();
void print_video_out_menu();
void print_video_out_type_menu();
void print_scanlines_mode_menu();
void print_buffering_mode_menu();
void print_cap_sync_mode_menu();
void print_capture_frequency_menu();
void print_ext_clk_divider_menu();
void print_video_sync_mode_menu();
void print_image_tuning_menu();
void print_pin_inversion_mask_menu();
void print_test_menu();

// Configuration print functions
void print_video_out_type();
void print_video_out_mode();
void print_scanlines_mode();
void print_buffering_mode();
void print_cap_sync_mode();
void print_capture_frequency();
void print_ext_clk_divider();
void print_capture_delay();
void print_x_offset();
void print_y_offset();
void print_dividers();
void print_video_sync_mode();
void print_pin_inversion_mask();
void print_settings();

// Main menu handling function
void handle_serial_menu();

// Utility functions
void print_byte_hex(uint8_t);
void binary_to_string(uint8_t, bool, char *);
uint32_t string_to_int(const char *);
char get_menu_input(int);