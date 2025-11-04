#pragma once

#ifndef OSD_FONT_H
#define OSD_FONT_H

#include <Arduino.h>

// 8x8 bitmap font data
// Each character is 8 bytes, representing 8x8 pixels
// Bit 7 = leftmost pixel, Bit 0 = rightmost pixel

extern const uint8_t osd_font_8x8[256][8];

// Font rendering functions
void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y, 
                   char c, uint8_t fg_color, uint8_t bg_color);
void osd_draw_string(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y, 
                     const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_draw_string_centered(uint8_t *buffer, uint16_t buf_width, uint16_t y, 
                              const char *str, uint8_t fg_color, uint8_t bg_color);

#endif // OSD_FONT_H