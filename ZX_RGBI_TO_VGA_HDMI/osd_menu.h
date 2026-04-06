#pragma once

// OSD Menu-specific definitions
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
#define MENU_TYPE_FF_OSD 6

// Menu-specific OSD state extension
typedef struct
{
    uint8_t selected_item;       // Currently selected menu item
    bool tuning_mode;            // True when adjusting parameters, false for navigation
    uint8_t original_video_mode; // Store original mode when entering video mode tuning
} osd_menu_state_t;

typedef struct
{
    uint8_t current_menu;
    uint8_t menu_depth;
    uint8_t menu_stack[4]; // Support 4 levels of nested menus
    uint8_t item_stack[4];
} osd_menu_nav_t;

extern osd_menu_nav_t osd_menu;
extern osd_menu_state_t osd_menu_state;

// Menu functions
void osd_menu_init();
void osd_menu_update();
void osd_menu_toggle();
bool osd_menu_buttons_blocked();

void osd_update_text_buffer(); // Update text buffer based on current menu state

void osd_adjust_image_parameter(uint8_t param_index, int8_t direction);
void osd_adjust_video_mode(int8_t direction);
void osd_adjust_capture_parameter(uint8_t param_index, int8_t direction);
void osd_adjust_ff_osd_parameter(uint8_t param_index, int8_t direction);
