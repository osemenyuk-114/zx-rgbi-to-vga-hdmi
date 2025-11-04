#include "osd.h"
#include "g_config.h"
#include "video_output.h"
#include "rgb_capture.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <stdio.h>
#include <string.h>

// External settings access
extern settings_t settings;
extern video_out_type_t active_video_output;

// Global OSD state
osd_state_t osd_state = {
    .enabled = false,
    .visible = false,
    .x_pos = 40,   // Moved to the left side
    .y_pos = 80,   // Centered vertically
    .needs_redraw = true,
    .selected_item = 0  // Start with first menu item selected
};

osd_buttons_t osd_buttons = {0};
osd_menu_nav_t osd_menu = {0};
uint8_t osd_buffer[OSD_BUFFER_SIZE];

// Debounce timing - increased for slower navigation
#define DEBOUNCE_TIME_US 250000  // 250ms debounce (slower cursor movement)
#define REPEAT_DELAY_US 500000  // 500ms initial repeat delay
#define REPEAT_RATE_US 100000   // 100ms repeat rate

void osd_init() {
    #ifdef OSD_MENU_ENABLED
    // Initialize OSD state
    memset(&osd_state, 0, sizeof(osd_state));
    memset(&osd_buttons, 0, sizeof(osd_buttons));
    memset(&osd_menu, 0, sizeof(osd_menu));
    
    // Initialize menu navigation
    osd_menu.current_menu = MENU_TYPE_MAIN;
    osd_menu.menu_depth = 0;
    
    // Set left position for better layout (byte-aligned for optimal rendering)
    osd_state.x_pos = 40;   // Moved to the left side (even number = byte-aligned)
    osd_state.y_pos = 80;   // Centered vertically
    osd_state.enabled = true;
    osd_state.needs_redraw = true;
    osd_state.selected_item = 0;
    osd_state.tuning_mode = false;
    
    // Initialize buttons
    osd_buttons_init();
    
    // Clear overlay buffer
    osd_clear_buffer();
    #endif
}

void osd_buttons_init() {
    #ifdef OSD_MENU_ENABLED
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
    for (int i = 0; i < 3; i++) {
        osd_buttons.last_press_time[i] = current_time;
    }
    
    // Initialize menu timeout tracking
    osd_state.last_activity_time = current_time;
    osd_state.show_time = current_time;
    
    // Menu starts hidden - user presses SEL to show
    #endif
}

void osd_buttons_update() {
    #ifdef OSD_MENU_ENABLED
    if (!osd_state.enabled) return;
    
    uint32_t current_time = time_us_32();
    
    // Read button states (buttons are active LOW with pull-up)
    bool button_states[3] = {
        !gpio_get(OSD_BTN_UP),
        !gpio_get(OSD_BTN_DOWN),
        !gpio_get(OSD_BTN_SEL)
    };
    
    bool* button_pressed[3] = {
        &osd_buttons.up_pressed,
        &osd_buttons.down_pressed,
        &osd_buttons.sel_pressed
    };
    
    // Update each button with repeat functionality
    for (int i = 0; i < 3; i++) {
        if (button_states[i]) {
            // Button is currently pressed
            if (!osd_buttons.key_held[i]) {
                // First press detection
                if (current_time - osd_buttons.last_press_time[i] > DEBOUNCE_TIME_US) {
                    *button_pressed[i] = true;
                    osd_buttons.key_held[i] = true;
                    osd_buttons.key_hold_start[i] = current_time;
                    osd_buttons.last_repeat_time[i] = current_time;
                    osd_buttons.last_press_time[i] = current_time;
                }
            } else {
                // Key is held - check for repeat
                uint32_t hold_duration = current_time - osd_buttons.key_hold_start[i];
                uint32_t since_last_repeat = current_time - osd_buttons.last_repeat_time[i];
                
                // Initial repeat delay, then accelerating repeat rate
                uint32_t repeat_delay;
                if (hold_duration < REPEAT_DELAY_US) {
                    repeat_delay = REPEAT_DELAY_US; // Initial delay
                } else {
                    repeat_delay = REPEAT_RATE_US;  // Faster repeat
                }
                
                if (since_last_repeat > repeat_delay) {
                    *button_pressed[i] = true;
                    osd_buttons.last_repeat_time[i] = current_time;
                }
            }
        } else {
            // Button released
            *button_pressed[i] = false;
            osd_buttons.key_held[i] = false;
        }
    }
    #endif
}

bool osd_button_pressed(uint8_t button) {
    #ifdef OSD_MENU_ENABLED
    switch (button) {
        case 0: return osd_buttons.up_pressed;
        case 1: return osd_buttons.down_pressed;
        case 2: return osd_buttons.sel_pressed;
        default: return false;
    }
    #else
    return false;
    #endif
}

void osd_update() {
    #ifdef OSD_MENU_ENABLED
    if (!osd_state.enabled) return;
    
    // Update button states
    osd_buttons_update();
    
    // Check for menu timeout (only when visible)
    if (osd_state.visible) {
        uint64_t current_time = time_us_64();
        if ((current_time - osd_state.last_activity_time) > OSD_MENU_TIMEOUT_US) {
            osd_hide();
            return;
        }
    }
    
    // Handle menu toggle (only when menu is not visible)
    if (osd_button_pressed(2) && !osd_state.visible) { // SEL button
        osd_toggle();
        // Reset button state to prevent multiple toggles
        osd_buttons.sel_pressed = false;
    }
    
    // Handle menu navigation if visible
    if (osd_state.visible) {
        // Get menu item count based on current menu
        uint8_t max_items;
        if (osd_menu.current_menu == MENU_TYPE_MAIN) {
            max_items = 3; // Main menu: 0-3 (4 items)
        } else if (osd_menu.current_menu == MENU_TYPE_VIDEO_OUTPUT) {
            max_items = (settings.video_out_type == DVI) ? 2 : 5; // DVI: 0-2 (3 items), VGA: 0-5 (6 items)
        } else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST) {
            max_items = 5; // Image adjust menu: 0-5 (6 items: H-POS, V-POS, DELAY, FREQUENCY, RESET, BACK)
        } else {
            max_items = 0;
        }
        
        // Handle navigation buttons
        if (osd_button_pressed(0)) { // UP button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_state.tuning_mode && osd_state.selected_item < 4) {
                // Parameter adjustment mode - increase parameter value
                osd_adjust_image_parameter(osd_state.selected_item, 1);
                osd_state.needs_redraw = true;
            } else {
                // Menu navigation mode - move selection up
                if (osd_state.selected_item > 0) {
                    osd_state.selected_item--;
                    osd_state.needs_redraw = true;
                }
            }
            osd_buttons.up_pressed = false;
        }
        if (osd_button_pressed(1)) { // DOWN button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_state.tuning_mode && osd_state.selected_item < 4) {
                // Parameter adjustment mode - decrease parameter value
                osd_adjust_image_parameter(osd_state.selected_item, -1);
                osd_state.needs_redraw = true;
            } else {
                // Menu navigation mode - move selection down
                if (osd_state.selected_item < max_items) {
                    osd_state.selected_item++;
                    osd_state.needs_redraw = true;
                }
            }
            osd_buttons.down_pressed = false;
        }
        
        // Handle selection in different menus
        if (osd_button_pressed(2) && osd_state.selected_item >= 0) { // SEL button for item selection
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_MAIN) {
                // Main menu selection
                if (osd_state.selected_item == 0) { // Video Output
                    // Enter video output submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_VIDEO_OUTPUT;
                    osd_state.selected_item = 0;
                    osd_state.needs_redraw = true;
                } else if (osd_state.selected_item == 2) { // Image Adjust
                    // Enter image adjust submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_IMAGE_ADJUST;
                    osd_state.selected_item = 0;
                    osd_state.tuning_mode = false; // Start in navigation mode
                    osd_state.needs_redraw = true;
                } else if (osd_state.selected_item == 3) { // Save Settings
                    extern void save_settings(settings_t* settings);
                    save_settings(&settings);
                    osd_hide(); // Hide menu after saving settings
                } else {
                    // Handle other main menu items here
                }
            } else if (osd_menu.current_menu == MENU_TYPE_VIDEO_OUTPUT) {
                // Video output submenu selection
                uint8_t back_item_index = (settings.video_out_type == DVI) ? 2 : 5;
                
                if (osd_state.selected_item == back_item_index) { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0) {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.needs_redraw = true;
                    }
                } else {
                    // Video mode selection - apply the change
                    video_out_mode_t new_mode = MODE_640x480_60Hz; // Default fallback
                    uint8_t old_mode = settings.video_out_mode;
                    
                    if (settings.video_out_type == DVI) {
                        switch (osd_state.selected_item) {
                            case 0: new_mode = MODE_640x480_60Hz; break;
                            case 1: new_mode = MODE_720x576_50Hz; break;
                        }
                    } else { // VGA
                        switch (osd_state.selected_item) {
                            case 0: new_mode = MODE_640x480_60Hz; break;
                            case 1: new_mode = MODE_800x600_60Hz; break;
                            case 2: new_mode = MODE_1024x768_60Hz; break;
                            case 3: new_mode = MODE_1280x1024_60Hz_d3; break;
                            case 4: new_mode = MODE_1280x1024_60Hz_d4; break;
                        }
                    }
                    
                    settings.video_out_mode = new_mode;
                    
                    // Apply video mode change like in serial menu
                    if (old_mode != settings.video_out_mode && active_video_output == settings.video_out_type) {
                        // Check if new mode is OSD compatible (div=2 only)
                        video_mode_t new_video_mode = *(video_modes[settings.video_out_mode]);
                        bool osd_compatible = (new_video_mode.div == 2);
                        
                        stop_video_output();
                        start_video_output(active_video_output);
                        // Adjust capture frequency for new system clock
                        set_capture_frequency(settings.frequency);
                    }
                }
            } else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST) {
                // Image adjust submenu selection
                uint8_t back_item_index = 5; // 6 items: H-POS, V-POS, DELAY, FREQUENCY, RESET, BACK
                
                if (osd_state.selected_item == back_item_index) { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0) {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_state.tuning_mode = false; // Reset tuning mode
                        osd_state.needs_redraw = true;
                    }
                } else if (osd_state.selected_item == 4) { // Reset to defaults
                    set_capture_shX(shX_DEF);
                    set_capture_shY(shY_DEF);
                    set_capture_delay(DELAY_DEF);
                    set_capture_frequency(FREQUENCY_DEF);
                    osd_state.needs_redraw = true;
                    osd_hide(); // Hide menu after resetting to defaults
                } else if (osd_state.selected_item < 4) { // Adjustable parameters (0-3)
                    // Toggle tuning mode for adjustable parameters
                    osd_state.tuning_mode = !osd_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            } else {
                // If we're in an unknown menu or want to close, toggle off
                osd_toggle();
            }
            osd_buttons.sel_pressed = false;
        }
        
        if (osd_state.needs_redraw) {
            osd_render_menu();
            osd_state.needs_redraw = false;
        }
    }
    #endif
}

void osd_show() {
    #ifdef OSD_MENU_ENABLED
    if (osd_state.enabled) {
        uint64_t current_time = time_us_64();
        osd_state.visible = true;
        osd_state.needs_redraw = true;
        osd_state.show_time = current_time;
        osd_state.last_activity_time = current_time;
    }
    #endif
}

// Update activity time when user interacts with menu
void osd_update_activity() {
    #ifdef OSD_MENU_ENABLED
    osd_state.last_activity_time = time_us_64();
    #endif
}

void osd_hide() {
    #ifdef OSD_MENU_ENABLED
    osd_state.visible = false;
    #endif
}

void osd_toggle() {
    #ifdef OSD_MENU_ENABLED
    if (osd_state.visible) {
        osd_hide();
    } else {
        osd_show();
    }
    #endif
}

void osd_clear_buffer() {
    #ifdef OSD_MENU_ENABLED
    // Fill with background color (2 pixels per byte)
    uint8_t bg_color_pair = OSD_COLOR_BACKGROUND | (OSD_COLOR_BACKGROUND << 4);
    memset(osd_buffer, bg_color_pair, OSD_BUFFER_SIZE);
    #endif
}

void osd_set_position(uint16_t x, uint16_t y) {
    #ifdef OSD_MENU_ENABLED
    osd_state.x_pos = x & ~1;  // Force byte alignment (even x position)
    osd_state.y_pos = y;
    osd_state.needs_redraw = true;
    #endif
}

void osd_render_menu() {
    #ifdef OSD_MENU_ENABLED
    // Clear the overlay buffer
    osd_clear_buffer();
    
    // Draw menu background
    for (int y = 0; y < OSD_HEIGHT; y++) {
        for (int x = 0; x < OSD_WIDTH; x += 2) {
            int buffer_offset = y * (OSD_WIDTH / 2) + (x / 2);
            if (buffer_offset >= OSD_BUFFER_SIZE) continue;
            
            uint8_t color_left = OSD_COLOR_BACKGROUND;
            uint8_t color_right = OSD_COLOR_BACKGROUND;
            
            // Draw border
            if (y == 0 || y == OSD_HEIGHT-1 || x == 0 || x == OSD_WIDTH-1) {
                color_left = OSD_COLOR_BORDER;
            }
            if (y == 0 || y == OSD_HEIGHT-1 || x+1 == 0 || x+1 == OSD_WIDTH-1) {
                color_right = OSD_COLOR_BORDER;
            }
            
            // Set pixel pair (2 pixels per byte)
            osd_buffer[buffer_offset] = color_left | (color_right << 4);
        }
    }
    
    // Draw test menu content - ZX Spectrum 8-pixel line spacing
    osd_draw_string_centered(osd_buffer, OSD_WIDTH, 8, "ZX RGBI CONVERTER", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND);
    osd_draw_string_centered(osd_buffer, OSD_WIDTH, 16, "SETUP MENU", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);
    
    // Menu items with selection indicator
    const char* menu_items[] = {
        "VIDEO OUTPUT",
        "SYNC MODE", 
        "IMAGE ADJUST",
        "SAVE SETTINGS"
    };
    
    // Dynamic video output submenu items based on video output type
    const char* video_output_items_dvi[] = {
        "640X480 @60HZ",
        "720X576 @50HZ",
        "< BACK TO MAIN"
    };
    
    const char* video_output_items_vga[] = {
        "640X480 @60HZ",
        "800X600 @60HZ", 
        "1024X768 @60HZ",
        "1280X1024 @60HZ (DIV3)",
        "1280X1024 @60HZ (DIV4)",
        "< BACK TO MAIN"
    };
    
    // Draw menu based on current menu type
    if (osd_menu.current_menu == MENU_TYPE_MAIN) {
        // Main menu
        for (int i = 0; i < 4; i++) {
            uint8_t y_pos = 32 + (i * 12);  // Start menu items at 12-pixel intervals for better spacing
            uint8_t color = (i == osd_state.selected_item) ? OSD_COLOR_SELECTED : OSD_COLOR_TEXT;
            
            // Draw prefix and menu item separately to avoid snprintf issues
            if (i == osd_state.selected_item) {
                osd_draw_string(osd_buffer, OSD_WIDTH, 8, y_pos, ">", color, OSD_COLOR_BACKGROUND);
                osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, menu_items[i], color, OSD_COLOR_BACKGROUND);
            } else {
                osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, menu_items[i], color, OSD_COLOR_BACKGROUND);
            }
        }
    } else if (osd_menu.current_menu == MENU_TYPE_VIDEO_OUTPUT) {
        // Video output submenu - select items based on video output type
        const char** current_items;
        uint8_t item_count;
        
        if (settings.video_out_type == DVI) {
            current_items = video_output_items_dvi;
            item_count = 3; // 2 modes + back
        } else {
            current_items = video_output_items_vga;
            item_count = 6; // 5 modes + back
        }
        
        osd_draw_string_centered(osd_buffer, OSD_WIDTH, 20, "VIDEO OUTPUT MODE", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);
        
        for (int i = 0; i < item_count; i++) {
            uint8_t y_pos = 32 + (i * 12);  // Start menu items at 12-pixel intervals for better spacing
            uint8_t color = (i == osd_state.selected_item) ? OSD_COLOR_SELECTED : OSD_COLOR_TEXT;
            
            // Draw prefix and menu item separately to avoid snprintf issues
            if (i == osd_state.selected_item) {
                osd_draw_string(osd_buffer, OSD_WIDTH, 8, y_pos, ">", color, OSD_COLOR_BACKGROUND);
                osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, current_items[i], color, OSD_COLOR_BACKGROUND);
            } else {
                osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, current_items[i], color, OSD_COLOR_BACKGROUND);
            }
        }
    } else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST) {
        // Image adjust submenu
        const char* image_adjust_items[] = {
            "HORIZONTAL POSITION",
            "VERTICAL POSITION", 
            "DELAY",
            "FREQUENCY",
            "RESET TO DEFAULTS",
            "< BACK TO MAIN"
        };
        
        osd_draw_string_centered(osd_buffer, OSD_WIDTH, 20, "IMAGE ADJUST", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND);
        
        for (int i = 0; i < 6; i++) {
            uint8_t y_pos = 32 + (i * 12);  // Start menu items at 12-pixel intervals for better spacing
            uint8_t color = (i == osd_state.selected_item) ? OSD_COLOR_SELECTED : OSD_COLOR_TEXT;
            
            // Draw prefix and menu item separately to avoid snprintf issues
            if (i == osd_state.selected_item) {
                // Show mode indicator: ">" for navigation, ">>" for tuning
                if (i < 4 && osd_state.tuning_mode) {
                    osd_draw_string(osd_buffer, OSD_WIDTH, 8, y_pos, ">>", color, OSD_COLOR_BACKGROUND);
                } else {
                    osd_draw_string(osd_buffer, OSD_WIDTH, 8, y_pos, ">", color, OSD_COLOR_BACKGROUND);
                }
                
                // Show parameter values for adjustable items
                if (i < 4) {
                    char value_str[40];  // Increased buffer size for longer parameter names
                    switch (i) {
                        case 0: // Horizontal Position
                            snprintf(value_str, sizeof(value_str), "H_POS: %d", settings.shX);
                            break;
                        case 1: // Vertical Position
                            snprintf(value_str, sizeof(value_str), "V_POS: %d", settings.shY);
                            break;
                        case 2: // Delay
                            snprintf(value_str, sizeof(value_str), "DELAY: %d", settings.delay);
                            break;
                        case 3: // Frequency
                            snprintf(value_str, sizeof(value_str), "FREQ: %lu", settings.frequency);
                            break;
                    }
                    osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, value_str, color, OSD_COLOR_BACKGROUND);
                } else {
                    osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, image_adjust_items[i], color, OSD_COLOR_BACKGROUND);
                }
            } else {
                osd_draw_string(osd_buffer, OSD_WIDTH, 24, y_pos, image_adjust_items[i], color, OSD_COLOR_BACKGROUND);
            }
        }
    }
    #endif
}

// Image parameter adjustment function
void osd_adjust_image_parameter(uint8_t param_index, int8_t direction) {
    #ifdef OSD_MENU_ENABLED
    switch (param_index) {
        case 0: // Horizontal Position (shX)
            if (direction > 0 && settings.shX < shX_MAX) {
                set_capture_shX(settings.shX + 1);
            } else if (direction < 0 && settings.shX > shX_MIN) {
                set_capture_shX(settings.shX - 1);
            }
            break;
            
        case 1: // Vertical Position (shY)
            if (direction > 0 && settings.shY < shY_MAX) {
                set_capture_shY(settings.shY + 1);
            } else if (direction < 0 && settings.shY > shY_MIN) {
                set_capture_shY(settings.shY - 1);
            }
            break;
            
        case 2: // Delay
            if (direction > 0 && settings.delay < DELAY_MAX) {
                settings.delay++;
                set_capture_delay(settings.delay);
            } else if (direction < 0 && settings.delay > DELAY_MIN) {
                settings.delay--;
                set_capture_delay(settings.delay);
            }
            break;
            
        case 3: // Frequency
            {
                uint32_t freq_step = 100; // Base step: 100Hz
                uint32_t current_time = time_us_32();
                
                // Determine which button is being used (UP or DOWN)
                uint8_t button_index = (direction > 0) ? 0 : 1; // 0=UP, 1=DOWN
                
                // Calculate step size based on key hold duration
                if (osd_buttons.key_held[button_index]) {
                    uint32_t hold_duration = current_time - osd_buttons.key_hold_start[button_index];
                    
                    // Progressive step increase based on hold time
                    if (hold_duration > 5000000) {      // After 5 seconds: 100kHz steps
                        freq_step = 100000;
                    } else if (hold_duration > 2000000) { // After 2 seconds: 10kHz steps
                        freq_step = 10000;
                    } else if (hold_duration > 1000000) { // After 1 second: 1kHz steps  
                        freq_step = 1000;
                    } else {                            // First second: 100Hz steps
                        freq_step = 100;
                    }
                }
                
                if (direction > 0 && settings.frequency < FREQUENCY_MAX - freq_step) {
                    settings.frequency += freq_step;
                } else if (direction < 0 && settings.frequency > FREQUENCY_MIN + freq_step) {
                    settings.frequency -= freq_step;
                }
                // Apply frequency change immediately
                set_capture_frequency(settings.frequency);
            }
            break;
    }
    #endif
}

// Note: Direct OSD buffer access is now used in video output modules for performance
// The osd_state and osd_buffer are accessed directly from VGA/DVI scanline generation