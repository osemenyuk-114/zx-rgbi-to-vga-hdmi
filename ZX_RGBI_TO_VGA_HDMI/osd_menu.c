#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"

#include "g_config.h"
#include "osd_menu.h"
#include "rgb_capture.h"
#include "settings.h"
#include "video_output.h"

// Debounce timing - increased for slower navigation
#define DEBOUNCE_TIME_US 250000 // 250ms debounce (slower cursor movement)
#define REPEAT_DELAY_US 500000  // 500ms initial repeat delay
#define REPEAT_RATE_US 100000   // 100ms repeat rate

// Pin inversion mask bit positions for menu items
// Bit mapping: F(6), SSI(4), KSI(5), I(3), R(2), G(1), B(0)
static const uint8_t mask_bit_positions[] = {6, 4, 5, 3, 2, 1, 0};

extern settings_t settings;
extern video_out_type_t active_video_output;
extern volatile bool restart_capture;

osd_state_t osd_state = {
    .enabled = false,
    .visible = false,
    .needs_redraw = true,
    .selected_item = 0 // Start with first menu item selected
};

osd_buttons_t osd_buttons = {0};
osd_menu_nav_t osd_menu = {0};
uint8_t osd_buffer[OSD_BUFFER_SIZE];
char osd_text_buffer[OSD_TEXT_BUFFER_SIZE];
uint8_t osd_text_colors[OSD_TEXT_BUFFER_SIZE]; // High nibble: fg_color, Low nibble: bg_color

const uint8_t osd_font_8x8[256][8] = {
    //
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00},
    ['"'] = {0x00, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#'] = {0x00, 0x24, 0x7E, 0x24, 0x24, 0x7E, 0x24, 0x00},
    ['$'] = {0x00, 0x08, 0x3E, 0x28, 0x3E, 0x0A, 0x3E, 0x08},
    ['%'] = {0x00, 0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00},
    ['&'] = {0x00, 0x10, 0x28, 0x10, 0x2A, 0x44, 0x3A, 0x00},
    ['\''] = {0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['('] = {0x00, 0x04, 0x08, 0x08, 0x08, 0x08, 0x04, 0x00},
    [')'] = {0x00, 0x20, 0x10, 0x10, 0x10, 0x10, 0x20, 0x00},
    ['*'] = {0x00, 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00},
    ['+'] = {0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x10},
    ['-'] = {0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/'] = {0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00},
    // Numbers 48-57 (0-9)
    ['0'] = {0x00, 0x3C, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00},
    ['1'] = {0x00, 0x18, 0x28, 0x08, 0x08, 0x08, 0x3E, 0x00},
    ['2'] = {0x00, 0x3C, 0x42, 0x02, 0x3C, 0x40, 0x7E, 0x00},
    ['3'] = {0x00, 0x3C, 0x42, 0x0C, 0x02, 0x42, 0x3C, 0x00},
    ['4'] = {0x00, 0x08, 0x18, 0x28, 0x48, 0x7E, 0x08, 0x00},
    ['5'] = {0x00, 0x7E, 0x40, 0x7C, 0x02, 0x42, 0x3C, 0x00},
    ['6'] = {0x00, 0x3C, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00},
    ['7'] = {0x00, 0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00},
    ['8'] = {0x00, 0x3C, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00},
    ['9'] = {0x00, 0x3C, 0x42, 0x42, 0x3E, 0x02, 0x3C, 0x00},
    // Punctuation and symbols
    [':'] = {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00},
    [';'] = {0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x20},
    ['<'] = {0x00, 0x00, 0x04, 0x08, 0x10, 0x08, 0x04, 0x00},
    ['='] = {0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00},
    ['>'] = {0x00, 0x00, 0x10, 0x08, 0x04, 0x08, 0x10, 0x00},
    ['?'] = {0x00, 0x3C, 0x42, 0x04, 0x08, 0x00, 0x08, 0x00},
    ['@'] = {0x00, 0x3C, 0x4A, 0x56, 0x5E, 0x40, 0x3C, 0x00},
    // Letters A-Z (65-90)
    ['A'] = {0x00, 0x3C, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00},
    ['B'] = {0x00, 0x7C, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00},
    ['C'] = {0x00, 0x3C, 0x42, 0x40, 0x40, 0x42, 0x3C, 0x00},
    ['D'] = {0x00, 0x78, 0x44, 0x42, 0x42, 0x44, 0x78, 0x00},
    ['E'] = {0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00},
    ['F'] = {0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00},
    ['G'] = {0x00, 0x3C, 0x42, 0x40, 0x4E, 0x42, 0x3C, 0x00},
    ['H'] = {0x00, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['I'] = {0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00},
    ['J'] = {0x00, 0x02, 0x02, 0x02, 0x42, 0x42, 0x3C, 0x00},
    ['K'] = {0x00, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00},
    ['L'] = {0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00},
    ['M'] = {0x00, 0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x00},
    ['N'] = {0x00, 0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x00},
    ['O'] = {0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['P'] = {0x00, 0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x00},
    ['Q'] = {0x00, 0x3C, 0x42, 0x42, 0x52, 0x4A, 0x3C, 0x00},
    ['R'] = {0x00, 0x7C, 0x42, 0x42, 0x7C, 0x44, 0x42, 0x00},
    ['S'] = {0x00, 0x3C, 0x40, 0x3C, 0x02, 0x42, 0x3C, 0x00},
    ['T'] = {0x00, 0xFE, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
    ['U'] = {0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['V'] = {0x00, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00},
    ['W'] = {0x00, 0x42, 0x42, 0x42, 0x42, 0x5A, 0x24, 0x00},
    ['X'] = {0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00},
    ['Y'] = {0x00, 0x82, 0x44, 0x28, 0x10, 0x10, 0x10, 0x00},
    ['Z'] = {0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x00},
    // Lowercase letters (97-122)
    ['a'] = {0x00, 0x00, 0x38, 0x04, 0x3C, 0x44, 0x3C, 0x00},
    ['b'] = {0x00, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x3C, 0x00},
    ['c'] = {0x00, 0x00, 0x1C, 0x20, 0x20, 0x20, 0x1C, 0x00},
    ['d'] = {0x00, 0x04, 0x04, 0x3C, 0x44, 0x44, 0x3C, 0x00},
    ['e'] = {0x00, 0x00, 0x38, 0x44, 0x78, 0x40, 0x3C, 0x00},
    ['f'] = {0x00, 0x0C, 0x10, 0x18, 0x10, 0x10, 0x10, 0x00},
    ['g'] = {0x00, 0x00, 0x3C, 0x44, 0x44, 0x3C, 0x04, 0x38},
    ['h'] = {0x00, 0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x00},
    ['i'] = {0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x38, 0x00},
    ['j'] = {0x00, 0x04, 0x00, 0x04, 0x04, 0x04, 0x24, 0x18},
    ['k'] = {0x00, 0x20, 0x28, 0x30, 0x30, 0x28, 0x24, 0x00},
    ['l'] = {0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0C, 0x00},
    ['m'] = {0x00, 0x00, 0x68, 0x54, 0x54, 0x54, 0x54, 0x00},
    ['n'] = {0x00, 0x00, 0x78, 0x44, 0x44, 0x44, 0x44, 0x00},
    ['o'] = {0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00},
    ['p'] = {0x00, 0x00, 0x78, 0x44, 0x44, 0x78, 0x40, 0x40},
    ['q'] = {0x00, 0x00, 0x3C, 0x44, 0x44, 0x3C, 0x04, 0x06},
    ['r'] = {0x00, 0x00, 0x1C, 0x20, 0x20, 0x20, 0x20, 0x00},
    ['s'] = {0x00, 0x00, 0x38, 0x40, 0x38, 0x04, 0x78, 0x00},
    ['t'] = {0x00, 0x10, 0x38, 0x10, 0x10, 0x10, 0x0C, 0x00},
    ['u'] = {0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00},
    ['v'] = {0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00},
    ['w'] = {0x00, 0x00, 0x44, 0x54, 0x54, 0x54, 0x28, 0x00},
    ['x'] = {0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00},
    ['y'] = {0x00, 0x00, 0x44, 0x44, 0x44, 0x3C, 0x04, 0x38},
    ['z'] = {0x00, 0x00, 0x7C, 0x08, 0x10, 0x20, 0x7C, 0x00},
    ['['] = {0x00, 0x0E, 0x08, 0x08, 0x08, 0x08, 0x0E, 0x00},
    // Special characters
    ['\\'] = {0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x00},
    [']'] = {0x00, 0x70, 0x10, 0x10, 0x10, 0x10, 0x70, 0x00},
    ['^'] = {0x00, 0x10, 0x38, 0x54, 0x10, 0x10, 0x10, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    ['`'] = {0x00, 0x1C, 0x22, 0x78, 0x20, 0x20, 0x7E, 0x00},
    ['{'] = {0x00, 0x00, 0x08, 0x08, 0x76, 0x42, 0x42, 0x00},
    ['|'] = {0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00},
    ['}'] = {0x00, 0x42, 0x42, 0x76, 0x08, 0x08, 0x00, 0x00},
    ['~'] = {0x00, 0x14, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Border characters (128-135)
    [OSD_CHAR_BORDER_TL] = {0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, // Top-left corner
    [OSD_CHAR_BORDER_TR] = {0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // Top-right corner
    [OSD_CHAR_BORDER_BL] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF}, // Bottom-left corner
    [OSD_CHAR_BORDER_BR] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF}, // Bottom-right corner
    [OSD_CHAR_BORDER_T] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // Horizontal line - top
    [OSD_CHAR_BORDER_L] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},  // Vertical line - left
    [OSD_CHAR_BORDER_B] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  // Horizontal line - bottom
    [OSD_CHAR_BORDER_R] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},  // Vertical line - right
};

void osd_init()
{
    // Initialize OSD state
    memset(&osd_state, 0, sizeof(osd_state));
    memset(&osd_buttons, 0, sizeof(osd_buttons));
    memset(&osd_menu, 0, sizeof(osd_menu));

    // Initialize menu navigation
    osd_menu.current_menu = MENU_TYPE_MAIN;
    osd_menu.menu_depth = 0;

    osd_state.enabled = true;
    osd_state.needs_redraw = true;
    osd_state.text_updated = true;
    osd_state.selected_item = 0;
    osd_state.tuning_mode = false;
    osd_state.mask_bit_position = 0;

    // Initialize text buffer
    osd_clear_text_buffer();

    // Initialize buttons
    osd_buttons_init();

    // Clear overlay buffer
    osd_clear_buffer();
}

void osd_update()
{
    if (!osd_state.enabled)
        return;

    // Update button states
    osd_buttons_update();

    // Check for menu timeout (only when visible)
    if (osd_state.visible)
    {
        uint64_t current_time = time_us_64();

        if ((current_time - osd_state.last_activity_time) > OSD_MENU_TIMEOUT_US)
        {
            osd_hide();
            return;
        }
    }

    // Show menu on UP/DOWN button press when not visible
    if (!osd_state.visible && (osd_button_pressed(0) || osd_button_pressed(1)))
    {
        osd_show();
        // Don't process the button press that opened the menu
        osd_buttons.up_pressed = false;
        osd_buttons.down_pressed = false;
        return;
    }

    // Handle menu navigation if visible
    if (osd_state.visible)
    { // Get menu item count based on current menu
        uint8_t max_items;
        if (osd_menu.current_menu == MENU_TYPE_MAIN)
            max_items = 5; // Main menu: 0-5 (6 items: OUTPUT, CAPTURE, IMAGE ADJUST, ABOUT, SAVE, EXIT)
        else if (osd_menu.current_menu == MENU_TYPE_OUTPUT)
            max_items = 3; // Output menu: 0-3 (4 items: mode, scanlines, buffering, back)
        else if (osd_menu.current_menu == MENU_TYPE_CAPTURE)
            max_items = 5; // Capture menu: 0-5 (6 items: freq, mode, divider, sync, mask, back) - divider always shown but dimmed for SELF
        else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST)
            max_items = 4; // Image adjust menu: 0-4 (5 items: H-POS, V-POS, DELAY, RESET, BACK)
        else if (osd_menu.current_menu == MENU_TYPE_MASK)
            max_items = 7; // Mask menu: 0-7 (8 items: F, SSI, KSI, I, B, G, R, BACK)
        else if (osd_menu.current_menu == MENU_TYPE_ABOUT)
            max_items = 0; // About menu: 0 (1 item: BACK)
        else
            max_items = 0;

        // Handle navigation buttons
        if (osd_button_pressed(0))
        {                          // UP button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_state.tuning_mode && osd_state.selected_item == 0)
            { // Video mode adjustment
                osd_adjust_video_mode(1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_state.tuning_mode && osd_state.selected_item < 3)
            { // Parameter adjustment mode - increase parameter value
                osd_adjust_image_parameter(osd_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_state.tuning_mode && osd_state.selected_item != 1 && osd_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
            else
            { // Menu navigation mode - move selection up
                if (osd_state.selected_item > 0)
                {
                    osd_state.selected_item--;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.up_pressed = false;
        }
        if (osd_button_pressed(1))
        {                          // DOWN button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_state.tuning_mode && osd_state.selected_item == 0)
            { // Video mode adjustment
                osd_adjust_video_mode(-1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_state.tuning_mode && osd_state.selected_item < 3)
            { // Parameter adjustment mode - decrease parameter value
                osd_adjust_image_parameter(osd_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_state.tuning_mode && osd_state.selected_item != 1 && osd_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
            else
            { // Menu navigation mode - move selection down
                if (osd_state.selected_item < max_items)
                {
                    osd_state.selected_item++;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.down_pressed = false;
        }

        // Handle selection in different menus
        if (osd_button_pressed(2) && osd_state.selected_item >= 0)
        {                          // SEL button for item selection
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_MAIN)
            {
                // Main menu selection
                if (osd_state.selected_item == 0)
                { // Output Settings
                    // Enter output submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_OUTPUT;
                    osd_state.selected_item = 0;
                    osd_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 1)
                { // Capture Settings
                    // Enter capture submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_CAPTURE;
                    osd_state.selected_item = 0;
                    osd_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 2)
                { // Image Adjust
                    // Enter image adjust submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_IMAGE_ADJUST;
                    osd_state.selected_item = 0;
                    osd_state.tuning_mode = false; // Start in navigation mode
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 3)
                { // About
                    // Enter about submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_ABOUT;
                    osd_state.selected_item = 0;
                    osd_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 4)
                { // Save
                    save_settings(&settings);
                    osd_hide();
                }
                else if (osd_state.selected_item == 5)
                { // Exit without saving
                    osd_hide();
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_OUTPUT)
            {                                // Output submenu selection
                uint8_t back_item_index = 3; // 4 items (mode, scanlines, buffering, back)

                if (osd_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_state.selected_item == 0)
                { // Video mode selection - enter/exit tuning mode
                    if (osd_state.tuning_mode)
                    { // Exit tuning mode and apply video mode change
                        // Only restart if mode has changed
                        if (active_video_output == settings.video_out_type &&
                            osd_state.original_video_mode != settings.video_out_mode)
                        {
                            stop_video_output();
                            start_video_output(active_video_output);
                            // Adjust capture frequency for new system clock
                            set_capture_frequency(settings.frequency);
                        }
                        osd_state.tuning_mode = false;
                    }
                    else
                    { // Enter tuning mode - store original mode
                        osd_state.original_video_mode = settings.video_out_mode;
                        osd_state.tuning_mode = true;
                    }
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 1)
                { // Scanlines - toggle
                    // Only allow toggle for VGA and when scanlines are supported
                    bool scanlines_supported = false;
                    if (settings.video_out_type == VGA)
                    {
#ifdef LOW_RES_SCANLINE
                        // When LOW_RES_SCANLINE is defined, scanlines are supported for all div values
                        scanlines_supported = true;
#else
                        // When LOW_RES_SCANLINE is not defined, only support div 3 and 4
                        uint8_t div = video_modes[settings.video_out_mode]->div;
                        scanlines_supported = (div == 3 || div == 4);
#endif
                    }

                    if (scanlines_supported)
                    {
                        settings.scanlines_mode = !settings.scanlines_mode;
                        set_scanlines_mode();
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_state.selected_item == 2)
                { // Buffering - toggle between X1 and X3
                    settings.buffering_mode = !settings.buffering_mode;
                    extern void set_buffering_mode(bool);
                    set_buffering_mode(settings.buffering_mode);
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE)
            {                                // Capture submenu selection
                uint8_t back_item_index = 5; // Always 6 items (freq, mode, divider, sync, mask, back)

                if (osd_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_state.selected_item == 1)
                { // Capture mode - toggle between SELF and EXT
                    settings.cap_sync_mode = (settings.cap_sync_mode == SELF) ? EXT : SELF;
                    restart_capture = true;
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 2)
                { // Divider
                    // Only allow tuning mode if EXT mode
                    if (settings.cap_sync_mode == EXT)
                    {
                        osd_state.tuning_mode = !osd_state.tuning_mode;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_state.selected_item == 3)
                { // Sync - toggle between COMPOSITE and SEPARATE
                    settings.video_sync_mode = !settings.video_sync_mode;
                    set_video_sync_mode(settings.video_sync_mode);
                    osd_state.needs_redraw = true;
                }
                else if (osd_state.selected_item == 4)
                { // Mask - open mask submenu
                    // Enter mask submenu
                    if (osd_menu.menu_depth < 3)
                    {
                        osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                        osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                        osd_menu.menu_depth++;
                        osd_menu.current_menu = MENU_TYPE_MASK;
                        osd_state.selected_item = 0;
                        osd_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else
                { // Other adjustable parameters (0)
                    // Toggle tuning mode for adjustable parameters
                    osd_state.tuning_mode = !osd_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST)
            {                                // Image adjust submenu selection
                uint8_t back_item_index = 4; // 5 items: H-POS, V-POS, DELAY, RESET, BACK

                if (osd_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false; // Reset tuning mode
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_state.selected_item == 3)
                { // Reset to defaults
                    set_capture_shX(shX_DEF);
                    set_capture_shY(shY_DEF);
                    set_capture_delay(DELAY_DEF);
                    osd_state.needs_redraw = true;
                    osd_hide(); // Hide menu after resetting to defaults
                }
                else if (osd_state.selected_item < 3)
                { // Adjustable parameters (0-2)
                    // Toggle tuning mode for adjustable parameters
                    osd_state.tuning_mode = !osd_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_MASK)
            {                                // Mask submenu selection
                uint8_t back_item_index = 7; // 8 items: F, SSI, KSI, I, B, G, R, BACK

                if (osd_state.selected_item == back_item_index)
                { // Back to Capture
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else
                { // Toggle the selected mask bit
                    uint8_t bit_pos = mask_bit_positions[osd_state.selected_item];
                    uint8_t bit_mask = 1 << bit_pos;
                    settings.pin_inversion_mask ^= bit_mask;
                    // Restart capture with new mask
                    restart_capture = true;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_ABOUT)
            { // About submenu - only BACK button
                if (osd_state.selected_item == 0)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
            }
            else
            { // If we're in an unknown menu or want to close, toggle off
                osd_toggle();
            }
            osd_buttons.sel_pressed = false;
        }

        // Update text buffer when menu state changes
        if (osd_state.needs_redraw)
        {
            osd_update_text_buffer();
            osd_state.needs_redraw = false;
            osd_state.text_updated = true;
        }

        // Render text buffer to pixel buffer when needed
        if (osd_state.text_updated)
        {
            osd_render_text_to_buffer();
            osd_state.text_updated = false;
        }
    }
}

void osd_show()
{
    if (osd_state.enabled)
    {
        uint64_t current_time = time_us_64();
        osd_state.visible = true;
        osd_state.needs_redraw = true;
        osd_state.show_time = current_time;
        osd_state.last_activity_time = current_time;

        // Draw border once when menu is activated
        osd_draw_border();
    }
}

void osd_hide()
{
    osd_state.visible = false;
}

void osd_toggle()
{
    if (osd_state.visible)
        osd_hide();
    else
        osd_show();
}

void osd_update_activity()
{
    osd_state.last_activity_time = time_us_64();
}

void osd_buttons_init()
{
    // Configure button pins as inputs with pull-up
    gpio_init(OSD_BTN_UP);
    gpio_set_dir(OSD_BTN_UP, GPIO_IN);
    gpio_pull_up(OSD_BTN_UP);

    gpio_init(OSD_BTN_DOWN);
    gpio_set_dir(OSD_BTN_DOWN, GPIO_IN);
    gpio_pull_up(OSD_BTN_DOWN);

    gpio_init(OSD_BTN_SEL);
    gpio_set_dir(OSD_BTN_SEL, GPIO_IN);
    gpio_pull_up(OSD_BTN_SEL);

    // Initialize timing
    uint64_t current_time = time_us_64();

    for (int i = 0; i < 3; i++)
        osd_buttons.last_press_time[i] = current_time;

    osd_buttons.sel_long_press_triggered = false;

    // Initialize menu timeout tracking
    osd_state.last_activity_time = current_time;
    osd_state.show_time = current_time;
}

void osd_buttons_update()
{
    if (!osd_state.enabled)
        return;

    uint32_t current_time = time_us_32();

    // Read button states (buttons are active LOW with pull-up)
    bool button_states[3] = {
        !gpio_get(OSD_BTN_UP),
        !gpio_get(OSD_BTN_DOWN),
        !gpio_get(OSD_BTN_SEL)};

    bool *button_pressed[3] = {
        &osd_buttons.up_pressed,
        &osd_buttons.down_pressed,
        &osd_buttons.sel_pressed};

    // Update each button with repeat functionality
    for (int i = 0; i < 3; i++)
    {
        if (button_states[i])
        { // Button is currently pressed
            if (!osd_buttons.key_held[i])
            { // First press detection
                if (current_time - osd_buttons.last_press_time[i] > DEBOUNCE_TIME_US)
                {
                    *button_pressed[i] = true;
                    osd_buttons.key_held[i] = true;
                    osd_buttons.key_hold_start[i] = current_time;
                    osd_buttons.last_repeat_time[i] = current_time;
                    osd_buttons.last_press_time[i] = current_time;

                    // Reset long press trigger on new SEL press
                    if (i == 2)
                        osd_buttons.sel_long_press_triggered = false;
                }
            }
            else
            { // Key is held - check for repeat
                uint32_t hold_duration = current_time - osd_buttons.key_hold_start[i];

                // Check for SEL button long press (>5 seconds)
                if (i == 2 && hold_duration > 5000000 && !osd_buttons.sel_long_press_triggered)
                { // Trigger video output type toggle
                    osd_buttons.sel_long_press_triggered = true;

                    // Toggle video output type
                    video_out_type_t new_type = (settings.video_out_type == DVI) ? VGA : DVI;
                    settings.video_out_type = new_type;
                    settings.video_out_mode = VIDEO_OUT_MODE_DEF;

                    // Switch video output if different from current
                    if (active_video_output != settings.video_out_type)
                    {
                        stop_video_output();
                        start_video_output(settings.video_out_type);
                        // Adjust capture frequency for new system clock
                        set_capture_frequency(settings.frequency);
                    }

                    // Force menu redraw to show new output type
                    if (osd_state.visible)
                        osd_state.needs_redraw = true;

                    // Don't process normal SEL press after long press
                    osd_buttons.sel_pressed = false;
                    continue;
                }

                uint32_t since_last_repeat = current_time - osd_buttons.last_repeat_time[i];

                // Initial repeat delay, then accelerating repeat rate
                uint32_t repeat_delay;

                if (hold_duration < REPEAT_DELAY_US)
                    repeat_delay = REPEAT_DELAY_US; // Initial delay
                else
                    repeat_delay = REPEAT_RATE_US; // Faster repeat

                if (since_last_repeat > repeat_delay)
                {
                    *button_pressed[i] = true;
                    osd_buttons.last_repeat_time[i] = current_time;
                }
            }
        }
        else
        { // Button released
            *button_pressed[i] = false;
            osd_buttons.key_held[i] = false;
        }
    }
}

bool osd_button_pressed(uint8_t button)
{
    switch (button)
    {
    case 0:
        return osd_buttons.up_pressed;

    case 1:
        return osd_buttons.down_pressed;

    case 2:
        return osd_buttons.sel_pressed;

    default:
        return false;
    }
}

void osd_clear_buffer()
{ // Fill with background color (2 pixels per byte)
    uint8_t bg_color_pair = OSD_COLOR_BACKGROUND | (OSD_COLOR_BACKGROUND << 4);
    memset(osd_buffer, bg_color_pair, OSD_BUFFER_SIZE);
}

void osd_clear_text_buffer()
{ // Clear text buffer but preserve border positions
    uint8_t default_color = (OSD_COLOR_TEXT << 4) | OSD_COLOR_BACKGROUND;

    for (uint8_t line = 0; line < OSD_LINES; line++)
    {
        for (uint8_t col = 0; col < OSD_CHARS_PER_LINE; col++)
        {
            uint16_t pos = line * OSD_CHARS_PER_LINE + col;

            // Skip border positions (first and last row, first and last column)
            if (line == 0 || line == OSD_LINES - 1 || col == 0 || col == OSD_CHARS_PER_LINE - 1)
                continue;

            osd_text_buffer[pos] = ' ';
            osd_text_colors[pos] = default_color;
        }
    }
}

char *osd_text_get_line_ptr(uint8_t line)
{
    if (line >= OSD_LINES)
        return NULL;
    return &osd_text_buffer[line * OSD_CHARS_PER_LINE];
}

void osd_text_set_char(uint8_t line, uint8_t col, char c, uint8_t fg_color, uint8_t bg_color)
{
    if (line >= OSD_LINES || col >= OSD_CHARS_PER_LINE)
        return;

    uint16_t pos = line * OSD_CHARS_PER_LINE + col;
    osd_text_buffer[pos] = c;
    osd_text_colors[pos] = (fg_color << 4) | bg_color; // High nibble: fg, Low nibble: bg
}

void osd_text_print(uint8_t line, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color)
{
    if (line >= OSD_LINES)
        return;

    uint16_t line_start = line * OSD_CHARS_PER_LINE;
    uint16_t pos = line_start + col;
    uint8_t max_len = OSD_CHARS_PER_LINE - col;
    uint8_t packed_color = (fg_color << 4) | bg_color; // High nibble: fg, Low nibble: bg

    // Fill left padding with spaces (avoid column 0 - left border)
    for (uint8_t i = 1; i < col; i++)
    {
        osd_text_buffer[line_start + i] = ' ';
        osd_text_colors[line_start + i] = packed_color;
    }

    uint8_t i;
    // Copy string characters (avoid last column - right border)
    uint8_t effective_max_len = (col + max_len >= OSD_CHARS_PER_LINE) ? (OSD_CHARS_PER_LINE - col - 1) : max_len;
    for (i = 0; i < effective_max_len && str[i] != '\0'; i++)
    {
        osd_text_buffer[pos + i] = str[i];
        osd_text_colors[pos + i] = packed_color;
    }
    // Pad with spaces to fill the rest of the line (avoid last column - right border)
    for (; i < effective_max_len; i++)
    {
        osd_text_buffer[pos + i] = ' ';
        osd_text_colors[pos + i] = packed_color;
    }
}

void osd_text_print_centered(uint8_t line, const char *str, uint8_t fg_color, uint8_t bg_color)
{
    if (line >= OSD_LINES)
        return;

    uint8_t len = strlen(str);
    // Account for border columns (exclude first and last column)
    uint8_t available_width = OSD_CHARS_PER_LINE - 2;
    if (len > available_width)
        len = available_width;

    uint8_t col = 1 + (available_width - len) / 2; // Start at column 1 (after left border)
    uint8_t packed_color = (fg_color << 4) | bg_color;
    uint16_t pos = line * OSD_CHARS_PER_LINE;

    // Fill left padding with spaces (start at column 1 to preserve left border)
    for (uint8_t i = 1; i < col; i++)
    {
        osd_text_buffer[pos + i] = ' ';
        osd_text_colors[pos + i] = packed_color;
    }

    // Print centered text (this will also pad the right side, but won't overwrite right border)
    osd_text_print(line, col, str, fg_color, bg_color);
}

void osd_text_printf(uint8_t line, uint8_t col, uint8_t fg_color, uint8_t bg_color, const char *format, ...)
{
    if (line >= OSD_LINES)
        return;

    char temp[OSD_CHARS_PER_LINE + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    osd_text_print(line, col, temp, fg_color, bg_color);
}

void osd_draw_border()
{ // Top border
    osd_text_set_char(0, 0, OSD_CHAR_BORDER_TL, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    for (uint8_t col = 1; col < OSD_CHARS_PER_LINE - 1; col++)
        osd_text_set_char(0, col, OSD_CHAR_BORDER_T, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    osd_text_set_char(0, OSD_CHARS_PER_LINE - 1, OSD_CHAR_BORDER_TR, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    // Bottom border
    osd_text_set_char(OSD_LINES - 1, 0, OSD_CHAR_BORDER_BL, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    for (uint8_t col = 1; col < OSD_CHARS_PER_LINE - 1; col++)
        osd_text_set_char(OSD_LINES - 1, col, OSD_CHAR_BORDER_B, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    osd_text_set_char(OSD_LINES - 1, OSD_CHARS_PER_LINE - 1, OSD_CHAR_BORDER_BR, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    // Left and right borders
    for (uint8_t line = 1; line < OSD_LINES - 1; line++)
    {
        osd_text_set_char(line, 0, OSD_CHAR_BORDER_L, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
        osd_text_set_char(line, OSD_CHARS_PER_LINE - 1, OSD_CHAR_BORDER_R, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    }
}

void osd_render_text_to_buffer()
{
    // Render text buffer to pixel buffer
    // No need to clear - we're rendering every character which overwrites everything
    for (uint8_t line = 0; line < OSD_LINES; line++)
    {
        for (uint8_t col = 0; col < OSD_CHARS_PER_LINE; col++)
        {
            uint16_t pos = line * OSD_CHARS_PER_LINE + col;
            char c = osd_text_buffer[pos];
            uint8_t packed_color = osd_text_colors[pos];
            uint8_t fg_color = (packed_color >> 4) & 0x0F; // High nibble
            uint8_t bg_color = packed_color & 0x0F;        // Low nibble

            uint16_t x = col * OSD_FONT_WIDTH;
            uint16_t y = line * OSD_FONT_HEIGHT;
            osd_draw_char(osd_buffer, OSD_WIDTH, x, y, c, fg_color, bg_color);
        }
    }
}

// Menu rendering functions
static void render_main_menu()
{
    const char *items[] = {
        "OUTPUT SETTINGS",
        "CAPTURE SETTINGS",
        "IMAGE ADJUST",
        "ABOUT",
        "SAVE",
        "EXIT"};

    for (int i = 0; i < 6; i++)
    {
        uint8_t line = OSD_MENU_START_LINE + i;
        uint8_t fg_color, bg_color;

        if (i == osd_state.selected_item)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_TEXT;
        }
        else
        {
            fg_color = OSD_COLOR_TEXT;
            bg_color = OSD_COLOR_BACKGROUND;
        }

        if (i < 4)
            osd_text_printf(line, 2, fg_color, bg_color, "%-16s >", items[i]);
        else
            osd_text_print(line, 2, items[i], fg_color, bg_color);
    }
}

static void render_output_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_LINE, "OUTPUT SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    for (int i = 0; i < 4; i++)
    {
        uint8_t line = OSD_MENU_START_LINE + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 1)
        {
            if (settings.video_out_type == DVI)
                color = OSD_COLOR_DIMMED;
            else if (settings.video_out_type == VGA)
            {
#ifndef LOW_RES_SCANLINE
                uint8_t div = video_modes[settings.video_out_mode]->div;
                if (div != 3 && div != 4)
                    color = OSD_COLOR_DIMMED;
#endif
            }
        }

        if (i == 0 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_state.selected_item)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = color;
        }
        else
        {
            fg_color = color;
            bg_color = OSD_COLOR_BACKGROUND;
        }

        if (i == 0)
        {
            const char *mode_names_dvi[] = {"640X480@60", "720X576@50"};
            const char *mode_names_vga[] = {"640X480@60", "800X600@60", "1024X768@60",
                                            "1280X1024@60 DIV3", "1280X1024@60 DIV4"};
            const char *current_mode_name = "UNKNOWN";
            if (settings.video_out_type == DVI)
            {
                if (settings.video_out_mode == MODE_640x480_60Hz)
                    current_mode_name = mode_names_dvi[0];
                else if (settings.video_out_mode == MODE_720x576_50Hz)
                    current_mode_name = mode_names_dvi[1];
            }
            else
            {
                if (settings.video_out_mode == MODE_640x480_60Hz)
                    current_mode_name = mode_names_vga[0];
                else if (settings.video_out_mode == MODE_800x600_60Hz)
                    current_mode_name = mode_names_vga[1];
                else if (settings.video_out_mode == MODE_1024x768_60Hz)
                    current_mode_name = mode_names_vga[2];
                else if (settings.video_out_mode == MODE_1280x1024_60Hz_d3)
                    current_mode_name = mode_names_vga[3];
                else if (settings.video_out_mode == MODE_1280x1024_60Hz_d4)
                    current_mode_name = mode_names_vga[4];
            }
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "MODE", current_mode_name);
        }
        else if (i == 1)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "SCANLINES", settings.scanlines_mode ? "ON" : "OFF");
        else if (i == 2)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "BUFFERING", settings.buffering_mode ? "X3" : "X1");
        else if (i == 3)
            osd_text_print(line, 2, "< BACK TO MAIN", fg_color, bg_color);

        if (i == 0 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            osd_text_set_char(line, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_capture_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_LINE, "CAPTURE SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    for (int i = 0; i < 6; i++)
    {
        uint8_t line = OSD_MENU_START_LINE + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 2 && settings.cap_sync_mode != EXT)
            color = OSD_COLOR_DIMMED;

        if (i < 5 && i != 1 && i != 3 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_state.selected_item)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = color;
        }
        else
        {
            fg_color = color;
            bg_color = OSD_COLOR_BACKGROUND;
        }

        if (i == 0)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %lu", "FREQ", settings.frequency);
        else if (i == 1)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "MODE", settings.cap_sync_mode == SELF ? "SELF-SYNC" : "EXTERNAL");
        else if (i == 2)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %d", "DIVIDER", settings.ext_clk_divider);
        else if (i == 3)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "SYNC", settings.video_sync_mode ? "SEPARATE" : "COMPOSITE");
        else if (i == 4)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %s", "MASK", ">");
        else if (i == 5)
            osd_text_print(line, 2, "< BACK TO MAIN", fg_color, bg_color);

        if (i < 5 && i != 1 && i != 3 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            osd_text_set_char(line, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_image_adjust_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_LINE, "IMAGE ADJUST", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    for (int i = 0; i < 5; i++)
    {
        uint8_t line = OSD_MENU_START_LINE + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i < 3 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_state.selected_item)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = color;
        }
        else
        {
            fg_color = color;
            bg_color = OSD_COLOR_BACKGROUND;
        }

        if (i == 0)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %d", "H_POS", settings.shX);
        else if (i == 1)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %d", "V_POS", settings.shY);
        else if (i == 2)
            osd_text_printf(line, 2, fg_color, bg_color, "%-9s %d", "DELAY", settings.delay);
        else if (i == 3)
            osd_text_print(line, 2, "RESET TO DEFAULTS", fg_color, bg_color);
        else if (i == 4)
            osd_text_print(line, 2, "< BACK TO MAIN", fg_color, bg_color);

        if (i < 3 && i == osd_state.selected_item && osd_state.tuning_mode)
        {
            osd_text_set_char(line, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_mask_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_LINE, "PIN INVERSION MASK", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    const char *mask_items[] = {
        "F   (FREQ)",
        "SSI (HSYNC)",
        "KSI (VSYNC)",
        "I   (BRIGHT)",
        "R   (RED)",
        "G   (GREEN)",
        "B   (BLUE)",
        "< BACK TO CAPTURE"};

    for (int i = 0; i < 8; i++)
    {
        uint8_t line = OSD_MENU_START_LINE + i;
        uint8_t fg_color, bg_color;

        if (i == osd_state.selected_item)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_TEXT;
        }
        else
        {
            fg_color = OSD_COLOR_TEXT;
            bg_color = OSD_COLOR_BACKGROUND;
        }

        if (i < 7)
        {
            uint8_t bit_pos = mask_bit_positions[i];
            bool bit_value = (settings.pin_inversion_mask >> bit_pos) & 1;
            osd_text_printf(line, 2, fg_color, bg_color, "%-12s %s", mask_items[i], bit_value ? "ON" : "OFF");
        }
        else
        {
            osd_text_print(line, 2, mask_items[i], fg_color, bg_color);
        }
    }
}

static void render_about_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_LINE, "ABOUT", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    osd_text_printf(OSD_MENU_START_LINE, 2, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, "VERSION   %s", FW_VERSION);

    osd_text_print(OSD_MENU_START_LINE + 2, 2, "https://github.com/", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND);
    osd_text_print(OSD_MENU_START_LINE + 3, 2, "osemenyuk-114/", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND);
    osd_text_print(OSD_MENU_START_LINE + 4, 2, "zx-rgbi-to-vga-hdmi", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND);

    uint8_t fg_color, bg_color;
    if (osd_state.selected_item == 0)
    {
        fg_color = OSD_COLOR_BACKGROUND;
        bg_color = OSD_COLOR_TEXT;
    }
    else
    {
        fg_color = OSD_COLOR_TEXT;
        bg_color = OSD_COLOR_BACKGROUND;
    }
    osd_text_print(OSD_MENU_START_LINE + 6, 2, "< BACK TO MAIN", fg_color, bg_color);
}

void osd_update_text_buffer()
{ // Clear text buffer
    osd_clear_text_buffer();

    // Draw header
    const char *title = settings.video_out_type == VGA ? "ZX RGBI TO VGA CONVERTER" : "ZX RGBI TO HDMI CONVERTER";
    osd_text_print_centered(OSD_TITLE_LINE, title, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND);
    osd_text_print_centered(OSD_SUBTITLE_LINE, "SETUP MENU", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);

    // Render menu based on current menu type
    switch (osd_menu.current_menu)
    {
    case MENU_TYPE_MAIN:
        render_main_menu();
        break;
    case MENU_TYPE_OUTPUT:
        render_output_menu();
        break;
    case MENU_TYPE_CAPTURE:
        render_capture_menu();
        break;
    case MENU_TYPE_IMAGE_ADJUST:
        render_image_adjust_menu();
        break;
    case MENU_TYPE_MASK:
        render_mask_menu();
        break;
    case MENU_TYPE_ABOUT:
        render_about_menu();
        break;
    }
}

void osd_adjust_image_parameter(uint8_t param_index, int8_t direction)
{
    switch (param_index)
    {
    case 0: // Horizontal Position (shX)
        if (direction > 0 && settings.shX < shX_MAX)
            set_capture_shX(settings.shX + 1);
        else if (direction < 0 && settings.shX > shX_MIN)
            set_capture_shX(settings.shX - 1);

        break;

    case 1: // Vertical Position (shY)
        if (direction > 0 && settings.shY < shY_MAX)
            set_capture_shY(settings.shY + 1);
        else if (direction < 0 && settings.shY > shY_MIN)
            set_capture_shY(settings.shY - 1);

        break;

    case 2: // Delay
        if (direction > 0 && settings.delay < DELAY_MAX)
        {
            settings.delay++;
            set_capture_delay(settings.delay);
        }
        else if (direction < 0 && settings.delay > DELAY_MIN)
        {
            settings.delay--;
            set_capture_delay(settings.delay);
        }

        break;
    }
}

void osd_adjust_capture_parameter(uint8_t param_index, int8_t direction)
{
    switch (param_index)
    {
    case 0: // Frequency
    {
        uint32_t freq_step = 100; // Base step: 100Hz
        uint32_t current_time = time_us_32();

        // Determine which button is being used (UP or DOWN)
        uint8_t button_index = (direction > 0) ? 0 : 1; // 0=UP, 1=DOWN

        // Calculate step size based on key hold duration
        if (osd_buttons.key_held[button_index])
        {
            uint32_t hold_duration = current_time - osd_buttons.key_hold_start[button_index];

            // Progressive step increase based on hold time
            if (hold_duration > 5000000)
                freq_step = 100000; // After 5 seconds: 100kHz steps
            else if (hold_duration > 2000000)
                freq_step = 10000; // After 2 seconds: 10kHz steps
            else if (hold_duration > 1000000)
                freq_step = 1000; // After 1 second: 1kHz steps
            else
                freq_step = 100; // First second: 100Hz steps
        }

        if (direction > 0 && settings.frequency < FREQUENCY_MAX - freq_step)
            settings.frequency += freq_step;
        else if (direction < 0 && settings.frequency > FREQUENCY_MIN + freq_step)
            settings.frequency -= freq_step;
        // Apply frequency change immediately
        set_capture_frequency(settings.frequency);
    }

    break;

    case 1: // Capture mode (SELF / EXT)
        if (settings.cap_sync_mode == SELF)
            settings.cap_sync_mode = EXT;
        else
            settings.cap_sync_mode = SELF;
        // Restart capture with new mode
        restart_capture = true;
        break;

    case 2: // External clock divider (only when cap_sync_mode == EXT)
        if (settings.cap_sync_mode == EXT)
        {
            if (direction > 0 && settings.ext_clk_divider < EXT_CLK_DIVIDER_MAX)
                settings.ext_clk_divider++;
            else if (direction < 0 && settings.ext_clk_divider > EXT_CLK_DIVIDER_MIN)
                settings.ext_clk_divider--;
            // Restart capture with new divider
            restart_capture = true;
        }

        break;

    case 3: // Video sync mode (Composite / Separate)
        settings.video_sync_mode = !settings.video_sync_mode;
        break;

    case 4: // Pin inversion mask
        // UP/DOWN moves between bit positions (0-6, bit 7 is always 0)
        // When a bit is selected, it can be toggled
        if (direction > 0)
        {
            // Move to next bit position (right)
            if (osd_state.mask_bit_position < 6)
                osd_state.mask_bit_position++;
        }
        else if (direction < 0)
        {
            // Move to previous bit position (left)
            if (osd_state.mask_bit_position > 0)
                osd_state.mask_bit_position--;
        }

        break;
    }
}

void osd_adjust_video_mode(int8_t direction)
{
    video_out_mode_t modes_dvi[] = {MODE_640x480_60Hz, MODE_720x576_50Hz};
    video_out_mode_t modes_vga[] = {MODE_640x480_60Hz, MODE_800x600_60Hz, MODE_1024x768_60Hz,
                                    MODE_1280x1024_60Hz_d3, MODE_1280x1024_60Hz_d4};

    video_out_mode_t *modes;
    uint8_t mode_count;

    if (settings.video_out_type == DVI)
    {
        modes = modes_dvi;
        mode_count = 2;
    }
    else
    {
        modes = modes_vga;
        mode_count = 5;
    }

    // Find current mode index
    int8_t current_index = -1;
    for (uint8_t i = 0; i < mode_count; i++)
        if (settings.video_out_mode == modes[i])
        {
            current_index = i;
            break;
        }

    // Default to first mode if current mode not found
    if (current_index == -1)
        current_index = 0;

    // Calculate new index with wrapping
    int8_t new_index = current_index + direction;

    if (new_index < 0)
        new_index = mode_count - 1; // Wrap to last mode
    else if (new_index >= mode_count)
        new_index = 0; // Wrap to first mode

    // Update mode in settings (don't apply yet - wait for SEL press)
    settings.video_out_mode = modes[new_index];
}

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   char c, uint8_t fg_color, uint8_t bg_color)
{
    if (c < 0 || c > 255)
        return;

    const uint8_t *char_data = osd_font_8x8[(uint8_t)c];

    for (int row = 0; row < 8; row++)
    {
        uint8_t line = char_data[row];
        for (int col = 0; col < 8; col++)
        {
            uint16_t px = x + col;
            uint16_t py = y + row;

            // Check bounds
            if (px >= buf_width || py >= OSD_HEIGHT)
                continue;

            // Calculate buffer position (2 pixels per byte)
            int buffer_offset = py * (buf_width / 2) + (px / 2);

            if (buffer_offset >= OSD_BUFFER_SIZE)
                continue;

            // Determine pixel color
            uint8_t pixel_color = (line & (0x80 >> col)) ? fg_color : bg_color;

            // Set pixel in buffer (2 pixels per byte)
            if (px & 1)
            {
                // Odd pixel (upper 4 bits)
                buffer[buffer_offset] = (buffer[buffer_offset] & 0x0F) | (pixel_color << 4);
            }
            else
            {
                // Even pixel (lower 4 bits)
                buffer[buffer_offset] = (buffer[buffer_offset] & 0xF0) | (pixel_color & 0x0F);
            }
        }
    }
}

void osd_draw_string(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                     const char *str, uint8_t fg_color, uint8_t bg_color)
{
    uint16_t cur_x = x;
    while (*str && cur_x < buf_width - 8)
    {
        osd_draw_char(buffer, buf_width, cur_x, y, *str, fg_color, bg_color);
        cur_x += 8; // ZX Spectrum: 8 pixels per character (no extra spacing)
        str++;
    }
}

void osd_draw_string_centered(uint8_t *buffer, uint16_t buf_width, uint16_t y,
                              const char *str, uint8_t fg_color, uint8_t bg_color)
{
    int len = strlen(str);
    int start_x = (buf_width - (len * 8)) / 2; // ZX Spectrum: 8 pixels per character

    // Snap to 8-pixel grid
    start_x = (start_x / 8) * 8;

    if (start_x < 0)
        start_x = 0;

    osd_draw_string(buffer, buf_width, start_x, y, str, fg_color, bg_color);
}