#pragma once

// OSD dimensions
#define OSD_WIDTH 240
#define OSD_HEIGHT 120
#define OSD_BUFFER_SIZE (OSD_WIDTH * OSD_HEIGHT / 2)
#define OSD_FONT_WIDTH 8
#define OSD_FONT_HEIGHT 8
#define OSD_COLUMNS (OSD_WIDTH / OSD_FONT_WIDTH)
#define OSD_ROWS (OSD_HEIGHT / OSD_FONT_HEIGHT)
#define OSD_TEXT_BUFFER_SIZE (OSD_COLUMNS * OSD_ROWS)

#define OSD_COLOR_BACKGROUND 0x0 // Black
#define OSD_COLOR_TEXT 0xB       // Bright cyan
#define OSD_COLOR_DIMMED 0x3     // Cyan
#define OSD_COLOR_SELECTED 0xF   // Bright white
#define OSD_COLOR_BORDER 0x7     // White

// Long-hold threshold used by OSD activation and release blocker.
#define OSD_HOLD_US 1000000u

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t start_x;
    uint16_t end_x;
    uint16_t start_y;
    uint16_t end_y;
    uint16_t width;
    uint16_t height;
    uint16_t buffer_size;
    uint8_t rows;
    uint8_t columns;
    bool border_enabled;
    bool full_width;
    uint16_t text_buffer_size;
} osd_mode_t;

typedef struct
{
    bool enabled;
    bool visible;
    bool needs_redraw;
    bool text_updated;           // True when text buffer needs to be rendered to OSD buffer
    bool menu_active;            // True when OSD menu owns the display
    uint64_t last_activity_time; // Time of last user interaction
    uint64_t show_time;          // Time when OSD was shown
} osd_state_t;

typedef struct
{
    bool up_pressed;
    bool down_pressed;
    bool sel_pressed;
    uint64_t last_press_time[3]; // Debounce timing
    bool repeat_enabled;
    uint64_t key_hold_start[3];    // When key hold started
    uint64_t last_repeat_time[3];  // Last repeat trigger time
    bool key_held[3];              // Is key currently held down
    bool sel_long_press_triggered; // True if SEL long press (>5s) has been triggered
} osd_buttons_t;

extern osd_state_t osd_state;
extern osd_mode_t osd_mode;
extern osd_buttons_t osd_buttons;
extern const uint8_t (*osd_font)[8];
extern uint8_t osd_buffer[OSD_BUFFER_SIZE];
extern char osd_text_buffer[OSD_TEXT_BUFFER_SIZE];    // Text buffer for content
extern uint8_t osd_text_colors[OSD_TEXT_BUFFER_SIZE]; // High nibble: fg_color, Low nibble: bg_color
extern uint8_t osd_text_heights[OSD_ROWS];            // 0 = normal height, 1 = double height (per row)

void osd_init();
void osd_update(); // Main update function - handles both menu and FF OSD
void osd_set_position();
void osd_show();
void osd_hide();
void osd_update_activity();

void osd_clear_text_buffer();
void osd_render_text_to_buffer(); // Render text buffer to OSD pixel buffer

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   uint8_t c, uint8_t fg_color, uint8_t bg_color, uint8_t height);

void osd_text_print(uint8_t row, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height);
void osd_text_print_centered(uint8_t row, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height);
void osd_text_printf(uint8_t row, uint8_t col, uint8_t fg_color, uint8_t bg_color, uint8_t height, const char *format, ...);
void osd_text_set_char(uint8_t row, uint8_t col, uint8_t c, uint8_t fg_color, uint8_t bg_color);

void osd_buttons_init();
void osd_buttons_update();
bool osd_button_pressed(uint8_t button);
bool osd_button_held(uint8_t button);
uint64_t osd_button_hold_duration_us(uint8_t button, uint64_t current_time);
bool osd_any_button_held();
void osd_clear_pressed_buttons();
void osd_block_buttons_until_release();
bool osd_buttons_blocked();
bool osd_buttons_apply_release_block();
