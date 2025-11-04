#pragma once

#ifndef OSD_H
#define OSD_H

#include <Arduino.h>
#include "pico.h"
#include "pico/time.h"
#include "g_config.h"
#include "osd_font.h"

// OSD Menu build configuration
// Both OSD and Serial menu can coexist when OSD_MENU_ENABLED is defined
#ifdef OSD_MENU_ENABLED
    #define MENU_SYSTEM_OSD
#endif

// OSD dimensions
#define OSD_WIDTH 280
#define OSD_HEIGHT 120
#define OSD_BUFFER_SIZE (OSD_WIDTH * OSD_HEIGHT / 2)  // 16800 bytes (2 pixels per byte)

// OSD button pins
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

// OSD colors (using existing RGBI palette)
#define OSD_COLOR_BACKGROUND 0x0    // Black
#define OSD_COLOR_TEXT 0xF          // Bright white
#define OSD_COLOR_SELECTED 0x3      // Bright cyan
#define OSD_COLOR_BORDER 0x7        // White

// Menu timeout (in microseconds)
#define OSD_MENU_TIMEOUT_US 10000000  // 10 seconds

// Menu types
#define MENU_TYPE_MAIN 0
#define MENU_TYPE_VIDEO_OUTPUT 1
#define MENU_TYPE_IMAGE_ADJUST 2

// Font dimensions
#define OSD_FONT_WIDTH 8
#define OSD_FONT_HEIGHT 8
#define OSD_CHARS_PER_LINE (OSD_WIDTH / OSD_FONT_WIDTH)  // 25 characters
#define OSD_LINES (OSD_HEIGHT / OSD_FONT_HEIGHT)         // 10 lines

// OSD state structure
typedef struct {
    bool enabled;
    bool visible;
    uint16_t x_pos;
    uint16_t y_pos;
    bool needs_redraw;
    uint8_t selected_item;  // Currently selected menu item
    bool tuning_mode;       // True when adjusting parameters, false for navigation
    uint64_t last_activity_time;  // Time of last user interaction
    uint64_t show_time;           // Time when menu was shown
} osd_state_t;

// Button state structure
typedef struct {
    bool up_pressed;
    bool down_pressed;
    bool sel_pressed;
    uint32_t last_press_time[3];  // debounce timing
    bool repeat_enabled;
    uint32_t key_hold_start[3];   // when key hold started
    uint32_t last_repeat_time[3]; // last repeat trigger time
    bool key_held[3];             // is key currently held down
} osd_buttons_t;

// Menu navigation structure
typedef struct {
    uint8_t current_menu;
    uint8_t selected_item;
    uint8_t menu_depth;
    uint8_t menu_stack[4];  // Support 4 levels of nested menus
    uint8_t item_stack[4];
} osd_menu_nav_t;

// Global OSD variables
extern osd_state_t osd_state;
extern osd_buttons_t osd_buttons;
extern osd_menu_nav_t osd_menu;
extern uint8_t osd_buffer[OSD_BUFFER_SIZE];

// Core OSD functions
void osd_init();
void osd_update();
void osd_show();
void osd_hide();
void osd_toggle();
void osd_update_activity();

// Button handling
void osd_buttons_init();
void osd_buttons_update();
bool osd_button_pressed(uint8_t button);

// Overlay rendering functions
void osd_clear_buffer();
void osd_render_menu();
void osd_set_position(uint16_t x, uint16_t y);

// Image adjustment functions
void osd_adjust_image_parameter(uint8_t param_index, int8_t direction);

// Direct access for video output modules (for performance)
// These are accessed directly in VGA/DVI scanline generation

#endif // OSD_H
