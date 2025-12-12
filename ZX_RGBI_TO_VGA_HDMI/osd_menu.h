#pragma once

// OSD dimensions
#define OSD_WIDTH 240
#define OSD_HEIGHT 120
#define OSD_BUFFER_SIZE (OSD_WIDTH * OSD_HEIGHT / 2) // 14400 bytes (2 pixels per byte)

#define OSD_FONT_WIDTH 8
#define OSD_FONT_HEIGHT 8
#define OSD_COLUMNS (OSD_WIDTH / OSD_FONT_WIDTH)      // 30 characters
#define OSD_ROWS (OSD_HEIGHT / OSD_FONT_HEIGHT)       // 15 rows
#define OSD_TEXT_BUFFER_SIZE (OSD_COLUMNS * OSD_ROWS) // 450 bytes

#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

#define OSD_COLOR_BACKGROUND 0x0 // Black
#define OSD_COLOR_TEXT 0xB       // Bright cyan
#define OSD_COLOR_DIMMED 0x3     // Cyan
#define OSD_COLOR_SELECTED 0xF   // Bright white
#define OSD_COLOR_BORDER 0x7     // White

// Border characters (custom font entries in high ASCII range)
#define OSD_CHAR_BORDER_TL 128 // Top-left corner
#define OSD_CHAR_BORDER_TR 129 // Top-right corner
#define OSD_CHAR_BORDER_BL 130 // Bottom-left corner
#define OSD_CHAR_BORDER_BR 131 // Bottom-right corner
#define OSD_CHAR_BORDER_T 132  // Horizontal line - top
#define OSD_CHAR_BORDER_L 133  // Vertical line - left
#define OSD_CHAR_BORDER_B 134  // Horizontal line - bottom
#define OSD_CHAR_BORDER_R 135  // Vertical line - right

#define OSD_MENU_TIMEOUT_US 10000000 // 10 seconds

// Menu layout constants
#define OSD_TITLE_ROW 1
#define OSD_SUBTITLE_ROW 3
#define OSD_MENU_START_ROW 5

#define MENU_TYPE_MAIN 0
#define MENU_TYPE_OUTPUT 1
#define MENU_TYPE_CAPTURE 2
#define MENU_TYPE_IMAGE_ADJUST 3
#define MENU_TYPE_MASK 4
#define MENU_TYPE_ABOUT 5

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
    uint32_t key_hold_start[3];    // When key hold started
    uint32_t last_repeat_time[3];  // Last repeat trigger time
    bool key_held[3];              // Is key currently held down
    bool sel_long_press_triggered; // True if SEL long press (>5s) has been triggered
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
extern char osd_text_buffer[OSD_TEXT_BUFFER_SIZE];    // Text buffer for menu content
extern uint8_t osd_text_colors[OSD_TEXT_BUFFER_SIZE]; // High nibble: fg_color, Low nibble: bg_color
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
void osd_update_text_buffer();    // Update text buffer based on current menu state
void osd_render_text_to_buffer(); // Render text buffer to OSD pixel buffer
void osd_draw_border();           // Draw border using special characters

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   char c, uint8_t fg_color, uint8_t bg_color);

// Text buffer helpers
void osd_text_print(uint8_t row, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_text_print_centered(uint8_t row, const char *str, uint8_t fg_color, uint8_t bg_color);
void osd_text_printf(uint8_t row, uint8_t col, uint8_t fg_color, uint8_t bg_color, const char *format, ...);
void osd_text_set_char(uint8_t row, uint8_t col, char c, uint8_t fg_color, uint8_t bg_color);

void osd_adjust_image_parameter(uint8_t param_index, int8_t direction);
void osd_adjust_video_mode(int8_t direction);
void osd_adjust_capture_parameter(uint8_t param_index, int8_t direction);
