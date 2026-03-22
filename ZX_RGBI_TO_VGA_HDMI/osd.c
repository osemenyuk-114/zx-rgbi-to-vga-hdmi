#include <stdio.h>
#include <stdarg.h>

#include "hardware/timer.h"
#include "hardware/gpio.h"

#include "g_config.h"
#include "osd.h"
#include "font.h"

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"
#endif

#define DEBOUNCE_TIME_US 250000 // 250ms debounce (slower cursor movement)
#define REPEAT_DELAY_US 500000  // 500ms initial repeat delay
#define REPEAT_RATE_US 100000   // 100ms repeat rate

extern video_mode_t video_mode;
extern int16_t h_visible_area;
extern int16_t h_margin;
extern int16_t v_visible_area;
extern int16_t v_margin;

osd_state_t osd_state = {
    .enabled = false,
    .visible = false,
    .needs_redraw = true,
    .text_updated = true};

osd_mode_t osd_mode = {
    .x = 0,
    .y = 0,
    .start_x = 0,
    .end_x = OSD_WIDTH,
    .start_y = 0,
    .end_y = OSD_HEIGHT,
    .width = OSD_WIDTH,
    .height = OSD_HEIGHT,
    .buffer_size = OSD_BUFFER_SIZE,
    .columns = OSD_COLUMNS,
    .rows = OSD_ROWS,
    .border_enabled = true,
    .text_buffer_size = OSD_TEXT_BUFFER_SIZE};

uint8_t osd_buffer[OSD_BUFFER_SIZE];
char osd_text_buffer[OSD_TEXT_BUFFER_SIZE];
uint8_t osd_text_colors[OSD_TEXT_BUFFER_SIZE];
uint8_t osd_text_heights[OSD_ROWS];

osd_buttons_t osd_buttons = {0};

void osd_init()
{ // Initialize OSD state
    memset(&osd_state, 0, sizeof(osd_state));

    osd_state.enabled = true;
    osd_state.needs_redraw = true;
    osd_state.text_updated = true;
    // Initialize text buffer
    osd_clear_text_buffer();
    // Clear overlay buffer
    osd_clear_buffer();

    // Initialize buttons
    osd_buttons_init();

#ifdef OSD_MENU_ENABLE
    // Initialize menu-specific components
    osd_menu_init();
#endif
}

void osd_show()
{
    if (!osd_state.enabled)
        return;

    uint64_t current_time = time_us_64();
    osd_state.visible = true;
    osd_state.needs_redraw = true;
    osd_state.show_time = current_time;
    osd_state.last_activity_time = current_time;
    // Draw border once when OSD is activated
    osd_draw_border();
}

void osd_hide()
{
    osd_state.visible = false;
}

void osd_update_activity()
{
    osd_state.last_activity_time = time_us_64();
}

void osd_clear_buffer()
{ // Fill with background color (2 pixels per byte)
    uint8_t bg_color_pair = OSD_COLOR_BACKGROUND | (OSD_COLOR_BACKGROUND << 4);
    memset(osd_buffer, bg_color_pair, OSD_BUFFER_SIZE);
}

void osd_clear_text_buffer()
{ // Clear text buffer but preserve border positions
    uint8_t default_color = (OSD_COLOR_TEXT << 4) | OSD_COLOR_BACKGROUND;

    for (uint8_t row = 0; row < osd_mode.rows; row++)
    {
        for (uint8_t col = 0; col < osd_mode.columns; col++)
        {
            uint16_t pos = row * osd_mode.columns + col;
            // Skip border positions (first and last row, first and last column) only if borders are enabled
            if (osd_mode.border_enabled &&
                (row == 0 || row == osd_mode.rows - 1 || col == 0 || col == osd_mode.columns - 1))
                continue;

            osd_text_buffer[pos] = ' ';
            osd_text_colors[pos] = default_color;
        }

        osd_text_heights[row] = 0;
    }
}

void osd_text_set_char(uint8_t row, uint8_t col, char c, uint8_t fg_color, uint8_t bg_color)
{
    if (row >= osd_mode.rows || col >= osd_mode.columns)
        return;

    uint16_t pos = row * osd_mode.columns + col;
    osd_text_buffer[pos] = c;
    osd_text_colors[pos] = (fg_color << 4) | bg_color;
}

void osd_text_print(uint8_t row, uint8_t col, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height)
{
    if (row >= osd_mode.rows)
        return;
    // Set height for this row
    osd_text_heights[row] = height;

    uint16_t row_start = row * osd_mode.columns;
    uint16_t pos = row_start + col;
    uint8_t max_len = osd_mode.columns - col;
    uint8_t packed_color = (fg_color << 4) | bg_color;

    // Fill left padding with spaces (avoid column 0 only if borders are enabled)
    uint8_t start_col = osd_mode.border_enabled ? 1 : 0;
    for (uint8_t i = start_col; i < col; i++)
    {
        osd_text_buffer[row_start + i] = ' ';
        osd_text_colors[row_start + i] = packed_color;
    }

    uint8_t i;
    // Copy string characters (avoid last column only if borders are enabled)
    uint8_t effective_max_len;
    if (osd_mode.border_enabled)
        effective_max_len = (col + max_len >= osd_mode.columns) ? (osd_mode.columns - col - 1) : max_len;
    else
        effective_max_len = max_len;

    for (i = 0; i < effective_max_len && str[i] != '\0'; i++)
    {
        osd_text_buffer[pos + i] = str[i];
        osd_text_colors[pos + i] = packed_color;
    }
    // Pad with spaces to fill the rest of the row
    for (; i < effective_max_len; i++)
    {
        osd_text_buffer[pos + i] = ' ';
        osd_text_colors[pos + i] = packed_color;
    }
}

void osd_text_print_centered(uint8_t row, const char *str, uint8_t fg_color, uint8_t bg_color, uint8_t height)
{
    if (row >= osd_mode.rows)
        return;

    uint8_t len = strlen(str);
    // Account for border columns only if borders are enabled
    uint8_t available_width = osd_mode.border_enabled ? (osd_mode.columns - 2) : osd_mode.columns;
    if (len > available_width)
        len = available_width;

    uint8_t start_col = osd_mode.border_enabled ? 1 : 0;
    uint8_t col = start_col + (available_width - len) / 2;
    uint8_t packed_color = (fg_color << 4) | bg_color;
    uint16_t pos = row * osd_mode.columns;

    // Fill left padding with spaces (start after border if enabled)
    for (uint8_t i = start_col; i < col; i++)
    {
        osd_text_buffer[pos + i] = ' ';
        osd_text_colors[pos + i] = packed_color;
    }
    // Print centered text
    osd_text_print(row, col, str, fg_color, bg_color, height);
}

void osd_text_printf(uint8_t row, uint8_t col, uint8_t fg_color, uint8_t bg_color, uint8_t height, const char *format, ...)
{
    if (row >= osd_mode.rows)
        return;

    char temp[osd_mode.columns + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    osd_text_print(row, col, temp, fg_color, bg_color, height);
}

void osd_draw_border()
{
    if (!osd_mode.border_enabled)
        return;

    // Top border
    osd_text_set_char(0, 0, OSD_CHAR_BORDER_TL, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    for (uint8_t col = 1; col < osd_mode.columns - 1; col++)
        osd_text_set_char(0, col, OSD_CHAR_BORDER_T, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    osd_text_set_char(0, osd_mode.columns - 1, OSD_CHAR_BORDER_TR, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    // Bottom border
    osd_text_set_char(osd_mode.rows - 1, 0, OSD_CHAR_BORDER_BL, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    for (uint8_t col = 1; col < osd_mode.columns - 1; col++)
        osd_text_set_char(osd_mode.rows - 1, col, OSD_CHAR_BORDER_B, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);

    osd_text_set_char(osd_mode.rows - 1, osd_mode.columns - 1, OSD_CHAR_BORDER_BR, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    // Left and right borders
    for (uint8_t row = 1; row < osd_mode.rows - 1; row++)
    {
        osd_text_set_char(row, 0, OSD_CHAR_BORDER_L, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
        osd_text_set_char(row, osd_mode.columns - 1, OSD_CHAR_BORDER_R, OSD_COLOR_BORDER, OSD_COLOR_BACKGROUND);
    }
}

void osd_render_text_to_buffer()
{                          // Render text buffer to pixel buffer
    uint16_t y_offset = 0; // Accumulated Y offset for double-height rows

    for (uint8_t row = 0; row < osd_mode.rows; row++)
    {
        uint8_t height = osd_text_heights[row];

        for (uint8_t col = 0; col < osd_mode.columns; col++)
        {
            uint16_t pos = row * osd_mode.columns + col;
            char c = osd_text_buffer[pos];
            uint8_t packed_color = osd_text_colors[pos];
            uint8_t fg_color = (packed_color >> 4) & 0x0F;
            uint8_t bg_color = packed_color & 0x0F;

            uint16_t x = col * OSD_FONT_WIDTH;
            uint16_t y = row * OSD_FONT_HEIGHT + y_offset;

            osd_draw_char(osd_buffer, osd_mode.width, x, y, c, fg_color, bg_color, height);
        }
        // Add extra vertical space for double-height rows
        if (height)
            y_offset += OSD_FONT_HEIGHT;
    }
}

void osd_draw_char(uint8_t *buffer, uint16_t buf_width, uint16_t x, uint16_t y,
                   char c, uint8_t fg_color, uint8_t bg_color, uint8_t height)
{
    if (c < 0 || c > 255)
        return;

    const uint8_t *char_data = osd_font[(uint8_t)c];
    uint8_t height_multiplier = height ? 2 : 1;

    for (int row = 0; row < OSD_FONT_HEIGHT; row++)
    {
        uint8_t line = char_data[row];

        for (int pixel_row = 0; pixel_row < height_multiplier; pixel_row++)
        {
            uint16_t py = y + row * height_multiplier + pixel_row;

            for (int col = 0; col < OSD_FONT_WIDTH; col++)
            {
                uint16_t px = x + col;
                // Check bounds
                if (px >= buf_width || py >= osd_mode.height)
                    continue;
                // Calculate buffer position (2 pixels per byte)
                int buffer_offset = py * (buf_width / 2) + (px / 2);

                if (buffer_offset >= osd_mode.buffer_size)
                    continue;
                // Determine pixel color
                uint8_t pixel_color = (line & (0x80 >> col)) ? fg_color : bg_color;
                // Set pixel in buffer (2 pixels per byte)
                if (px & 1)
                { // Odd pixel (upper 4 bits)
                    buffer[buffer_offset] = (buffer[buffer_offset] & 0x0F) | (pixel_color << 4);
                }
                else
                { // Even pixel (lower 4 bits)
                    buffer[buffer_offset] = (buffer[buffer_offset] & 0xF0) | (pixel_color & 0x0F);
                }
            }
        }
    }
}

void osd_update()
{
#ifdef OSD_MENU_ENABLE
    // Handle menu OSD if FF OSD is not active
    osd_menu_update();
#endif
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

    // Handle button input
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
#ifdef OSD_MENU_ENABLE
                    osd_buttons.sel_long_press_triggered = true;
                    // Toggle video output type
                    extern settings_t settings;
                    extern video_out_type_t active_video_output;
                    extern void stop_video_output();
                    extern void start_video_output(video_out_type_t);
                    extern void set_capture_frequency(uint32_t);

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
#endif
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
