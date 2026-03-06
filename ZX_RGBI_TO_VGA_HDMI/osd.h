#pragma once

// OSD dimensions
#define OSD_WIDTH 320
#define OSD_HEIGHT 120
#define OSD_BUFFER_SIZE (OSD_WIDTH * OSD_HEIGHT / 2) // 19200 bytes (2 pixels per byte)

#define OSD_FONT_WIDTH 8
#define OSD_FONT_HEIGHT 8
#define OSD_COLUMNS (OSD_WIDTH / OSD_FONT_WIDTH)      // 40 characters
#define OSD_ROWS (OSD_HEIGHT / OSD_FONT_HEIGHT)       // 15 rows
#define OSD_TEXT_BUFFER_SIZE (OSD_COLUMNS * OSD_ROWS) // 600 bytes

#define OSD_COLOR_BACKGROUND 0x0 // Black
#define OSD_COLOR_TEXT 0xB       // Bright cyan
#define OSD_COLOR_DIMMED 0x3     // Cyan
#define OSD_COLOR_SELECTED 0xF   // Bright white
#define OSD_COLOR_BORDER 0x7     // White

// Button pins
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

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
    uint16_t text_buffer_size;
} osd_mode_t;

typedef struct
{
    bool enabled;
    bool visible;
    bool needs_redraw;
    bool text_updated;           // True when text buffer needs to be rendered to OSD buffer
    uint64_t last_activity_time; // Time of last user interaction
    uint64_t show_time;          // Time when OSD was shown
} osd_state_t;

typedef struct
{
    bool up_pressed;
    bool down_pressed;
    bool sel_pressed;
    uint32_t last_press_time[3]; // Debounce timing
    bool repeat_enabled;
    uint32_t key_hold_start[3];    // When key hold started
    uint32_t last_repeat_time[3];  // Last repeat trigger time
    bool key_held[3];              // Is key currently held down
    bool sel_long_press_triggered; // True if SEL long press (>5s) has been triggered
} osd_buttons_t;

extern osd_state_t osd_state;
extern osd_mode_t osd_mode;
extern osd_buttons_t osd_buttons;
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

void osd_clear_buffer();
void osd_clear_text_buffer();
void osd_render_text_to_buffer(); // Render text buffer to OSD pixel buffer
void osd_draw_border();           // Draw border using special characters

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   char c, uint8_t fg_color, uint8_t bg_color, uint8_t height);

void osd_text_print(uint8_t row, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height);
void osd_text_print_centered(uint8_t row, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height);
void osd_text_printf(uint8_t row, uint8_t col, uint8_t fg_color, uint8_t bg_color, uint8_t height, const char *format, ...);
void osd_text_set_char(uint8_t row, uint8_t col, char c, uint8_t fg_color, uint8_t bg_color);

void osd_buttons_init();
void osd_buttons_update();
bool osd_button_pressed(uint8_t button);
