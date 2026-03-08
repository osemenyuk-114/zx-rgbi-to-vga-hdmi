#include "hardware/gpio.h"
#include "hardware/timer.h"

#include "g_config.h"
#include "osd_menu.h"
#include "osd.h"
#include "rgb_capture.h"
#include "settings.h"
#include "video_output.h"

// Pin inversion mask bit positions for menu items
// Bit mapping: F(6), SSI(4), KSI(5), I(3), R(2), G(1), B(0)
static const uint8_t mask_bit_positions[] = {6, 4, 5, 3, 2, 1, 0};

extern settings_t settings;
extern video_out_type_t active_video_output;
extern volatile bool restart_capture;

osd_menu_state_t osd_menu_state = {
    .selected_item = 0,
    .tuning_mode = false};

osd_menu_nav_t osd_menu = {0};

void osd_menu_init()
{
    // Initialize menu-specific state
    memset(&osd_menu_state, 0, sizeof(osd_menu_state));
    memset(&osd_menu, 0, sizeof(osd_menu));

    // Initialize menu navigation
    osd_menu.current_menu = MENU_TYPE_MAIN;
    osd_menu.menu_depth = 0;

    osd_menu_state.selected_item = 0;
    osd_menu_state.tuning_mode = false;
    osd_menu_state.mask_bit_position = 0;
}

void osd_menu_update()
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
        osd_mode.x = 0;
        osd_mode.y = 0;
        osd_mode.columns = 30;
        osd_mode.rows = 15;
        osd_mode.border_enabled = true;
        osd_mode.width = osd_mode.columns * OSD_FONT_WIDTH;
        osd_mode.height = osd_mode.rows * OSD_FONT_HEIGHT;
        osd_mode.buffer_size = osd_mode.width * osd_mode.height / 2;

        osd_set_position();

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
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_menu_state.tuning_mode && osd_menu_state.selected_item == 0)
            { // Video mode adjustment
                osd_adjust_video_mode(1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 3)
            { // Parameter adjustment mode - increase parameter value
                osd_adjust_image_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_menu_state.tuning_mode && osd_menu_state.selected_item != 1 && osd_menu_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
            else
            { // Menu navigation mode - move selection up
                if (osd_menu_state.selected_item > 0)
                {
                    osd_menu_state.selected_item--;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.up_pressed = false;
        }
        if (osd_button_pressed(1))
        {                          // DOWN button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_menu_state.tuning_mode && osd_menu_state.selected_item == 0)
            { // Video mode adjustment
                osd_adjust_video_mode(-1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 3)
            { // Parameter adjustment mode - decrease parameter value
                osd_adjust_image_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_menu_state.tuning_mode && osd_menu_state.selected_item != 1 && osd_menu_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
            else
            { // Menu navigation mode - move selection down
                if (osd_menu_state.selected_item < max_items)
                {
                    osd_menu_state.selected_item++;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.down_pressed = false;
        }

        // Handle selection in different menus
        if (osd_button_pressed(2) && osd_menu_state.selected_item >= 0)
        {                          // SEL button for item selection
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_MAIN)
            {
                // Main menu selection
                if (osd_menu_state.selected_item == 0)
                { // Output Settings
                    // Enter output submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_OUTPUT;
                    osd_menu_state.selected_item = 0;
                    osd_menu_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 1)
                { // Capture Settings
                    // Enter capture submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_CAPTURE;
                    osd_menu_state.selected_item = 0;
                    osd_menu_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 2)
                { // Image Adjust
                    // Enter image adjust submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_IMAGE_ADJUST;
                    osd_menu_state.selected_item = 0;
                    osd_menu_state.tuning_mode = false; // Start in navigation mode
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 3)
                { // About
                    // Enter about submenu
                    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                    osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
                    osd_menu.menu_depth++;
                    osd_menu.current_menu = MENU_TYPE_ABOUT;
                    osd_menu_state.selected_item = 0;
                    osd_menu_state.tuning_mode = false;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 4)
                { // Save
                    save_settings(&settings);
                    osd_hide();
                }
                else if (osd_menu_state.selected_item == 5)
                { // Exit without saving
                    osd_hide();
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_OUTPUT)
            {                                // Output submenu selection
                uint8_t back_item_index = 3; // 4 items (mode, scanlines, buffering, back)

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_menu_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 0)
                { // Video mode selection - enter/exit tuning mode
                    if (osd_menu_state.tuning_mode)
                    { // Exit tuning mode and apply video mode change
                        // Only restart if mode has changed
                        if (active_video_output == settings.video_out_type &&
                            osd_menu_state.original_video_mode != settings.video_out_mode)
                        {
                            stop_video_output();
                            start_video_output(active_video_output);
                            // Adjust capture frequency for new system clock
                            set_capture_frequency(settings.frequency);
                        }
                        osd_menu_state.tuning_mode = false;
                    }
                    else
                    { // Enter tuning mode - store original mode
                        osd_menu_state.original_video_mode = settings.video_out_mode;
                        osd_menu_state.tuning_mode = true;
                    }
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 1)
                { // Scanlines - toggle
                    // Only allow toggle for VGA and when scanlines are supported
                    bool scanlines_supported = false;
                    if (settings.video_out_type == VGA)
                    {
#ifdef SCANLINES_ENABLE_LOW_RES
                        // When SCANLINES_ENABLE_LOW_RES is defined, scanlines are supported for all div values
                        scanlines_supported = true;
#else
                        // When SCANLINES_ENABLE_LOW_RES is not defined, only support div 3 and 4
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
                else if (osd_menu_state.selected_item == 2)
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

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_menu_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 1)
                { // Capture mode - toggle between SELF and EXT
                    settings.cap_sync_mode = (settings.cap_sync_mode == SELF) ? EXT : SELF;
                    restart_capture = true;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 2)
                { // Divider
                    // Only allow tuning mode if EXT mode
                    if (settings.cap_sync_mode == EXT)
                    {
                        osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 3)
                { // Sync - toggle between COMPOSITE and SEPARATE
                    settings.video_sync_mode = !settings.video_sync_mode;
                    set_video_sync_mode(settings.video_sync_mode);
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 4)
                { // Mask - open mask submenu
                    // Enter mask submenu
                    if (osd_menu.menu_depth < 3)
                    {
                        osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
                        osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
                        osd_menu.menu_depth++;
                        osd_menu.current_menu = MENU_TYPE_MASK;
                        osd_menu_state.selected_item = 0;
                        osd_menu_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else
                { // Other adjustable parameters (0)
                    // Toggle tuning mode for adjustable parameters
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST)
            {                                // Image adjust submenu selection
                uint8_t back_item_index = 4; // 5 items: H-POS, V-POS, DELAY, RESET, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_menu_state.tuning_mode = false; // Reset tuning mode
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 3)
                { // Reset to defaults
                    set_capture_shX(shX_DEF);
                    set_capture_shY(shY_DEF);
                    set_capture_delay(DELAY_DEF);
                    osd_state.needs_redraw = true;
                    osd_hide(); // Hide menu after resetting to defaults
                }
                else if (osd_menu_state.selected_item < 3)
                { // Adjustable parameters (0-2)
                    // Toggle tuning mode for adjustable parameters
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_MASK)
            {                                // Mask submenu selection
                uint8_t back_item_index = 7; // 8 items: F, SSI, KSI, I, B, G, R, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Capture
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_menu_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
                else
                { // Toggle the selected mask bit
                    uint8_t bit_pos = mask_bit_positions[osd_menu_state.selected_item];
                    uint8_t bit_mask = 1 << bit_pos;
                    settings.pin_inversion_mask ^= bit_mask;
                    // Restart capture with new mask
                    restart_capture = true;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_ABOUT)
            { // About submenu - only BACK button
                if (osd_menu_state.selected_item == 0)
                { // Back to Main
                    // Return to previous menu
                    if (osd_menu.menu_depth > 0)
                    {
                        osd_menu.menu_depth--;
                        osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
                        osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
                        osd_menu_state.tuning_mode = false;
                        osd_state.needs_redraw = true;
                    }
                }
            }
            else
            { // If we're in an unknown menu or want to close, toggle off
                osd_menu_toggle();
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

void osd_menu_toggle()
{
    if (osd_state.visible)
        osd_hide();
    else
        osd_show();
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
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t fg_color, bg_color;

        if (i == osd_menu_state.selected_item)
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
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-16s >", items[i]);
        else
            osd_text_print(row, 2, items[i], fg_color, bg_color, 0);
    }
}

static void render_output_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "OUTPUT SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 4; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 1)
        {
            if (settings.video_out_type == DVI)
                color = OSD_COLOR_DIMMED;
            else if (settings.video_out_type == VGA)
            {
#ifndef SCANLINES_ENABLE_LOW_RES
                uint8_t div = video_modes[settings.video_out_mode]->div;
                if (div != 3 && div != 4)
                    color = OSD_COLOR_DIMMED;
#endif
            }
        }

        if (i == 0 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_menu_state.selected_item)
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
            const char *mode_names_vga[] = {"640X480@60", "800X600@60",
                                            "1024X768@60 DIV3", "1024X768@60 DIV4",
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
                else if (settings.video_out_mode == MODE_1024x768_60Hz_d3)
                    current_mode_name = mode_names_vga[2];
                else if (settings.video_out_mode == MODE_1024x768_60Hz_d4)
                    current_mode_name = mode_names_vga[3];
                else if (settings.video_out_mode == MODE_1280x1024_60Hz_d3)
                    current_mode_name = mode_names_vga[4];
                else if (settings.video_out_mode == MODE_1280x1024_60Hz_d4)
                    current_mode_name = mode_names_vga[5];
            }
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MODE", current_mode_name);
        }
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "SCANLINES", settings.scanlines_mode ? "ON" : "OFF");
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "BUFFERING", settings.buffering_mode ? "X3" : "X1");
        else if (i == 3)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i == 0 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_capture_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "CAPTURE SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 6; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 2 && settings.cap_sync_mode != EXT)
            color = OSD_COLOR_DIMMED;

        if (i < 5 && i != 1 && i != 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_menu_state.selected_item)
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
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %lu", "FREQ", settings.frequency);
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MODE", settings.cap_sync_mode == SELF ? "SELF-SYNC" : "EXTERNAL");
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "DIVIDER", settings.ext_clk_divider);
        else if (i == 3)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "SYNC", settings.video_sync_mode ? "SEPARATE" : "COMPOSITE");
        else if (i == 4)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MASK", ">");
        else if (i == 5)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i < 5 && i != 1 && i != 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_image_adjust_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "IMAGE ADJUST", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 5; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i < 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            fg_color = OSD_COLOR_BACKGROUND;
            bg_color = OSD_COLOR_SELECTED;
        }
        else if (i == osd_menu_state.selected_item)
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
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "H_POS", settings.shX);
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "V_POS", settings.shY);
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "DELAY", settings.delay);
        else if (i == 3)
            osd_text_print(row, 2, "RESET TO DEFAULTS", fg_color, bg_color, 0);
        else if (i == 4)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i < 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
        {
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
        }
    }
}

static void render_mask_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "PIN INVERSION MASK", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

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
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t fg_color, bg_color;

        if (i == osd_menu_state.selected_item)
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
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-12s %s", mask_items[i], bit_value ? "ON" : "OFF");
        }
        else
        {
            osd_text_print(row, 2, mask_items[i], fg_color, bg_color, 0);
        }
    }
}

static void render_about_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "ABOUT", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    osd_text_printf(OSD_MENU_START_ROW, 2, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0, "VERSION   %s", FW_VERSION);

    osd_text_print(OSD_MENU_START_ROW + 2, 2, "https://github.com/", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print(OSD_MENU_START_ROW + 3, 2, "osemenyuk-114/", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print(OSD_MENU_START_ROW + 4, 2, "zx-rgbi-to-vga-hdmi-PICOSDK", OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);

    uint8_t fg_color, bg_color;
    if (osd_menu_state.selected_item == 0)
    {
        fg_color = OSD_COLOR_BACKGROUND;
        bg_color = OSD_COLOR_TEXT;
    }
    else
    {
        fg_color = OSD_COLOR_TEXT;
        bg_color = OSD_COLOR_BACKGROUND;
    }
    osd_text_print(OSD_MENU_START_ROW + 6, 2, "< BACK TO MAIN", fg_color, bg_color, 0);
}

void osd_update_text_buffer()
{ // Clear text buffer
    osd_clear_text_buffer();

    // Draw header
    const char *title = settings.video_out_type == VGA ? "ZX RGBI TO VGA CONVERTER" : "ZX RGBI TO HDMI CONVERTER";
    osd_text_print_centered(OSD_TITLE_ROW, title, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print_centered(OSD_SUBTITLE_ROW, "SETUP MENU", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

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

            // Progressive step increase based on hold time and current frequency alignment
            if (hold_duration > 5000000 && (settings.frequency % 100000 == 0))
                freq_step = 100000; // After 5 seconds and 100kHz alignment: 100kHz steps
            else if (hold_duration > 2000000 && (settings.frequency % 10000 == 0))
                freq_step = 10000; // After 2 seconds and 10kHz alignment: 10kHz steps
            else if (hold_duration > 1000000 && (settings.frequency % 1000 == 0))
                freq_step = 1000; // After 1 second and 1kHz alignment: 1kHz steps
            else
                freq_step = 100; // First second: 100Hz steps
        }

        if (direction > 0 && settings.frequency <= FREQUENCY_MAX - freq_step)
            settings.frequency += freq_step;
        else if (direction < 0 && settings.frequency >= FREQUENCY_MIN + freq_step)
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
            if (osd_menu_state.mask_bit_position < 6)
                osd_menu_state.mask_bit_position++;
        }
        else if (direction < 0)
        {
            // Move to previous bit position (left)
            if (osd_menu_state.mask_bit_position > 0)
                osd_menu_state.mask_bit_position--;
        }

        break;
    }
}

void osd_adjust_video_mode(int8_t direction)
{
    video_out_mode_t modes_dvi[] = {MODE_640x480_60Hz, MODE_720x576_50Hz};
    video_out_mode_t modes_vga[] = {MODE_640x480_60Hz, MODE_800x600_60Hz,
                                    MODE_1024x768_60Hz_d3, MODE_1024x768_60Hz_d4,
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
        mode_count = 6;
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
