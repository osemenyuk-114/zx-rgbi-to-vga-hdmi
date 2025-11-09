#pragma once

#include <Arduino.h>
#include "pico.h"
#include "pico/time.h"
#include "g_config.h"

// OSD dimensions
#define OSD_WIDTH 240
#define OSD_HEIGHT 120
#define OSD_BUFFER_SIZE (OSD_WIDTH * OSD_HEIGHT / 2) // 14400 bytes (2 pixels per byte)

#define OSD_FONT_WIDTH 8
#define OSD_FONT_HEIGHT 8
#define OSD_CHARS_PER_LINE (OSD_WIDTH / OSD_FONT_WIDTH)  // 30 characters
#define OSD_LINES (OSD_HEIGHT / OSD_FONT_HEIGHT)         // 15 lines
#define OSD_TEXT_BUFFER_SIZE (OSD_CHARS_PER_LINE * OSD_LINES) // 450 bytes

#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

#define OSD_COLOR_BACKGROUND 0x0 // Black
#define OSD_COLOR_TEXT 0xF       // Bright white
#define OSD_COLOR_SELECTED 0x3   // Bright cyan
#define OSD_COLOR_BORDER 0x7     // White
#define OSD_COLOR_DIMMED 0x7     // White/gray (for disabled items)

#define OSD_MENU_TIMEOUT_US 10000000 // 10 seconds

#define MENU_TYPE_MAIN 0
#define MENU_TYPE_OUTPUT 1
#define MENU_TYPE_CAPTURE 2
#define MENU_TYPE_IMAGE_ADJUST 3
#define MENU_TYPE_MASK 4

typedef enum
{
    MENU_ITEM_SUBMENU, // Opens a submenu
    MENU_ITEM_TOGGLE,  // Toggle parameter (bool or enum)
    MENU_ITEM_RANGE,   // Adjustable range parameter
    MENU_ITEM_ACTION,  // Executes an action
    MENU_ITEM_BACK     // Returns to previous menu
} menu_item_type_t;

typedef struct
{
    bool enabled;
    bool visible;
    uint16_t x_pos;
    uint16_t y_pos;
    bool needs_redraw;
    bool text_updated;                  // True when text buffer needs to be rendered to OSD buffer
    uint8_t selected_item;              // Currently selected menu item
    bool tuning_mode;                   // True when adjusting parameters, false for navigation
    uint8_t original_video_mode;        // Store original mode when entering video mode tuning
    uint8_t mask_bit_position;          // Currently selected bit position for mask editing (0-6, bit 7 always 0)
    uint8_t mask_last_toggled_position; // Last bit position that was toggled (for exit detection)
    uint64_t last_activity_time;        // Time of last user interaction
    uint64_t show_time;                 // Time when menu was shown
} osd_state_t;

typedef struct
{
    bool up_pressed;
    bool down_pressed;
    bool sel_pressed;
    uint32_t last_press_time[3]; // Debounce timing
    bool repeat_enabled;
    uint32_t key_hold_start[3];   // When key hold started
    uint32_t last_repeat_time[3]; // Last repeat trigger time
    bool key_held[3];             // Is key currently held down
} osd_buttons_t;

typedef struct
{
    uint8_t current_menu;
    uint8_t selected_item;
    uint8_t menu_depth;
    uint8_t menu_stack[4]; // Support 4 levels of nested menus
    uint8_t item_stack[4];
} osd_menu_nav_t;

extern osd_state_t osd_state;
extern osd_buttons_t osd_buttons;
extern osd_menu_nav_t osd_menu;
extern uint8_t osd_buffer[OSD_BUFFER_SIZE];
extern char osd_text_buffer[OSD_TEXT_BUFFER_SIZE];      // Text buffer for menu content
extern uint8_t osd_text_colors[OSD_TEXT_BUFFER_SIZE];   // High nibble: fg_color, Low nibble: bg_color
extern const uint8_t osd_font_8x8[256][8];

void osd_init();
void osd_update();
void osd_show();
void osd_hide();
void osd_toggle();
void osd_update_activity();

void osd_buttons_init();
void osd_buttons_update();
bool osd_button_pressed(uint8_t button);

void osd_clear_buffer();
void osd_clear_text_buffer();
void osd_update_text_buffer();      // Update text buffer based on current menu state
void osd_render_text_to_buffer();   // Render text buffer to OSD pixel buffer
void osd_set_position(uint16_t x, uint16_t y);

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   char c, uint8_t fg_color, uint8_t bg_color);
void osd_draw_string(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                     const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_draw_string_centered(uint8_t *buffer, uint16_t buf_width, uint16_t y,
                              const char *str, uint8_t fg_color, uint8_t bg_color);

// Text buffer helpers
void osd_text_print(uint8_t line, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_text_print_centered(uint8_t line, const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_text_printf(uint8_t line, uint8_t col, uint8_t fg_color, uint8_t bg_color, const char *format, ...);
char* osd_text_get_line_ptr(uint8_t line);
void osd_text_set_char(uint8_t line, uint8_t col, char c, uint8_t fg_color, uint8_t bg_color);

void osd_adjust_image_parameter(uint8_t param_index, int8_t direction);
void osd_adjust_video_mode(int8_t direction);
void osd_adjust_capture_parameter(uint8_t param_index, int8_t direction);

uint8_t osd_get_menu_item_count(uint8_t menu_type);
bool osd_is_item_enabled(uint8_t menu_type, uint8_t item_index);
uint8_t osd_get_back_item_index(uint8_t menu_type);
const char *osd_get_video_mode_name(video_out_mode_t mode, video_out_type_t type);
void osd_format_menu_item_value(uint8_t menu_type, uint8_t item_index, char *buffer, size_t buffer_size);