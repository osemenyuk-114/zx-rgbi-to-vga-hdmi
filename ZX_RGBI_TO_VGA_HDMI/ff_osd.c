#include <memory.h>
#include <stddef.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "i2c_slave.h"

#include "g_config.h"
#include "ff_osd.h"
#include "font.h"
#include "osd.h"
#include "video_output.h"

#ifdef OSD_FF_ENABLE // Compile FF OSD code only if enabled in g_config.h to make it compatible with Arduino IDE builds that include ff_osd.c in all configurations

// Cross-core flag: set from core0 menus when FF OSD is enabled after being
// disabled at startup. Core1 loop picks this up and calls ff_osd_i2c_init().
volatile bool ff_osd_needs_i2c_init = false;

// Helper macros
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MASK(r, x) ((x) & (ARRAY_SIZE(r) - 1))

#define barrier() asm volatile("" ::: "memory")

static const uint I2C_SLAVE_SDA_PIN = I2C_PIN_SDA;
static const uint I2C_SLAVE_SCL_PIN = I2C_PIN_SCL;

// 100 kHz: I2C Standard Mode, matching flashfloppy setting
#define I2C_BAUDRATE 100000

#define FF_OSD_FW_VER "1.9"

// PCF8574 pin assignment: D7-D6-D5-D4-BL-EN-RW-RS
#define _BL (1u << 3)
#define _EN (1u << 2)
#define _RW (1u << 1)
#define _RS (1u << 0)

// FF OSD command set
#define FF_OSD_BACKLIGHT 0x00 // [0] = backlight on
#define FF_OSD_DATA 0x02      // next columns*rows bytes are text data
#define FF_OSD_ROWS 0x10      // [3:0] = #rows
#define FF_OSD_HEIGHTS 0x20   // [3:0] = 1 iff row is 2x height
#define FF_OSD_BUTTONS 0x30   // [3:0] = button mask
#define FF_OSD_COLUMNS 0x40   // [6:0] = #columns

#define FF_OSD_BUTTON_LEFT 1
#define FF_OSD_BUTTON_RIGHT 2
#define FF_OSD_BUTTON_SELECT 4

#define FF_OSD_BUTTON_PULSE_FRAMES 6

extern settings_t settings;

const char ff_osd_fw_ver[] = FF_OSD_FW_VER;

// Display state, exported to display routines
ff_osd_display_t ff_osd_display = {
    .cols = 20,
    .rows = 4,
    .heights = 0,
    .on = false,
    .text = {},
};

uint8_t ff_osd_buttons_rx; // button state: Gotek -> OSD

// SELECT forwarding state in FlashFloppy mode:
// - short tap: forwarded on release as a brief pulse
// - long hold: reserved for opening local OSD menu, not forwarded to Gotek
static bool ff_btn_prev_held = false;
static uint8_t ff_btn_pulse_frames = 0;

// state: OSD -> Gotek
ff_osd_info_t ff_osd_info = {
    .protocol_ver = 0,
    .fw_major = ff_osd_fw_ver[0],
    .fw_minor = ff_osd_fw_ver[2],
    .buttons = 0};

// I2C data ring
static uint8_t d_ring[1024];
static uint16_t d_cons, d_prod; // data ring buffer consumer / producer pointers

// Transaction ring: Data-ring offset of each transaction start
static uint16_t t_ring[8];
static uint16_t t_cons, t_prod; // transactions ring buffer consumer / producer pointers

// Current position in FF OSD I2C Protocol character data.
static uint8_t ff_osd_x, ff_osd_y;

// LCD state
static bool lcd_inc;
static uint8_t lcd_ddraddr;

uint16_t ff_osd_set_cols(int16_t cols)
{
    if (cols < FF_OSD_COLUMNS_MIN)
        return FF_OSD_COLUMNS_MIN;

    if (cols > FF_OSD_COLUMNS_MAX)
        return FF_OSD_COLUMNS_MAX;

    return cols;
}

uint8_t ff_osd_set_h_position(int8_t h_position)
{
    if (h_position < 1)
        return 1;

    if (h_position > 5)
        return 5;

    return h_position;
}

void ff_osd_set_buttons(uint8_t buttons)
{
    ff_osd_info.buttons = buttons;
}

static void lcd_display_update(void)
{
    if (settings.ff_osd_config.i2c_protocol)
        return;

    ff_osd_display.rows = settings.ff_osd_config.rows;
    ff_osd_display.cols = settings.ff_osd_config.cols;
    ff_osd_display.heights = 0;
    memset(ff_osd_display.text, ' ', sizeof(ff_osd_display.text));
}

static void __not_in_flash_func(lcd_process_cmd)(uint8_t cmd)
{
    uint8_t x = 0x80;
    int c = 0;

    if (!cmd)
        return;

    while (!(cmd & x))
    {
        x >>= 1;
        c++;
    }

    switch (c)
    {
    case 0: // Set DDR Address
        lcd_ddraddr = cmd & 127;
        break;

    case 1: // Set CGR Address
        break;

    case 2: // Function Set
        break;

    case 3: // Cursor or Display Shift
        break;

    case 4: // Display On/Off Control
        break;

    case 5: // Entry Mode Set
        lcd_inc = (cmd & 2) != 0;
        break;

    case 6: // Return Home
        lcd_ddraddr = 0;
        break;

    case 7: // Clear Display
        memset(ff_osd_display.text, ' ', sizeof(ff_osd_display.text));
        lcd_ddraddr = 0;
        break;
    }
}

static void __not_in_flash_func(lcd_process_dat)(uint8_t dat)
{
    int x, y;
    if (lcd_ddraddr >= 0x68)
        lcd_ddraddr = 0x00; // jump to line 2

    if ((lcd_ddraddr >= 0x28) && (lcd_ddraddr < 0x40))
        lcd_ddraddr = 0x40; // jump to line 1

    x = lcd_ddraddr & 0x3f;
    y = lcd_ddraddr >> 6;

    if ((ff_osd_display.rows == 4) && (x >= 20))
    {
        x -= 20;
        y += 2;
    }

    ff_osd_display.text[y][x] = dat;
    lcd_ddraddr++;

    if (x >= ff_osd_display.cols)
    {
        if (x + 1 > FF_OSD_COLUMNS_MAX)
            ff_osd_display.cols = FF_OSD_COLUMNS_MAX;
        else
            ff_osd_display.cols = x + 1;
    }
}

static void __not_in_flash_func(lcd_process)(void)
{
    uint16_t d_c, d_p = d_prod;
    static uint16_t dat = 1;
    static bool rs;

    // Process the command sequence.
    for (d_c = d_cons; d_c != d_p; d_c++)
    {
        uint8_t x = d_ring[MASK(d_ring, d_c)];

        if ((x & (_EN | _RW)) != _EN)
            continue;

        ff_osd_display.on = !!(x & _BL);

        if (rs != !!(x & _RS))
        {
            rs ^= 1;
            dat = 1;
        }

        dat <<= 4;
        dat |= x >> 4;

        if (dat & 0x100)
        {
            if (rs)
                lcd_process_dat(dat);
            else
                lcd_process_cmd(dat);

            dat = 1;
        }
    }

    d_cons = d_c;
}

static void __not_in_flash_func(ffosd_process)(void)
{
    uint16_t d_c, d_p, t_c, t_p;

    d_c = d_cons;
    d_p = d_prod;
    barrier(); // Get data ring producer /then/ transaction ring producer
    t_c = t_cons;
    t_p = t_prod;

    // We only care about the last full transaction, and newer.
    if ((uint16_t)(t_p - t_c) >= 2)
    {
        // Discard older transactions, and in-progress old transaction.
        t_c = t_p - 2;
        d_c = t_ring[MASK(t_ring, t_c)];
        ff_osd_x = 0;
        ff_osd_y = 0;
    }

    // Process the command sequence.
    for (; d_c != d_p; d_c++)
    {
        uint8_t x = d_ring[MASK(d_ring, d_c)];

        if ((t_c != t_p) && (d_c == t_ring[MASK(t_ring, t_c)]))
        {
            t_c++;
            ff_osd_y = 0;
        }

        if (ff_osd_y != 0)
        {
            // Character Data.
            ff_osd_display.text[ff_osd_y - 1][ff_osd_x] = x;

            if (++ff_osd_x >= ff_osd_display.cols)
            {
                ff_osd_x = 0;

                if (++ff_osd_y > ff_osd_display.rows)
                    ff_osd_y = 0;
            }
        }
        else
        {
            // Command.
            if ((x & 0xc0) == FF_OSD_COLUMNS)
            {
                // 0-40
                if ((x & 0x3f) > FF_OSD_COLUMNS_MAX)
                    ff_osd_display.cols = FF_OSD_COLUMNS_MAX;
                else
                    ff_osd_display.cols = x & 0x3f;
            }
            else
            {
                switch (x & 0xf0)
                {
                case FF_OSD_BUTTONS:
                    ff_osd_buttons_rx = x & 0x0f;
                    break;

                case FF_OSD_ROWS:
                    // 0-3
                    ff_osd_display.rows = x & 0x03;
                    break;

                case FF_OSD_HEIGHTS:
                    ff_osd_display.heights = x & 0x0f;
                    break;

                case FF_OSD_BACKLIGHT:
                    switch (x & 0x0f)
                    {
                    case 0:
                        ff_osd_display.on = false;
                        break;

                    case 1:
                        ff_osd_display.on = true;
                        break;

                    case 2:
                        ff_osd_x = 0;
                        ff_osd_y = 1;
                        break;
                    }
                }
            }
        }
    }

    d_cons = d_c;
    t_cons = t_c;
}

static void __not_in_flash_func(i2c_slave_handler)(i2c_inst_t *i2c, i2c_slave_event_t event)
{
    static uint8_t rp = 0;
    static volatile bool addr_matched = false;

    switch (event)
    {
    case I2C_SLAVE_RECEIVE: // master has written some data
        // On address match (first RECEIVE after FINISH), mark transaction start
        if (!addr_matched)
        {
            t_ring[MASK(t_ring, t_prod++)] = d_prod;
            rp = 0;
            addr_matched = true;
        }
        // Read incoming byte - ISR is called for each byte received
        d_ring[MASK(d_ring, d_prod++)] = i2c_read_byte_raw(i2c);
        break;

    case I2C_SLAVE_REQUEST: // master is requesting data
        // On address match for read operation, just reset read position
        if (!addr_matched)
        {
            rp = 0;
            addr_matched = true;
        }
        // Send response byte - ISR is called for each byte requested
        uint8_t *info = (uint8_t *)&ff_osd_info;
        i2c_write_byte_raw(i2c, (rp < sizeof(ff_osd_info)) ? info[rp++] : 0);
        break;

    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        // Transaction complete - reset for next transaction
        addr_matched = false;
        break;

    default:
        break;
    }
}

void ff_osd_i2c_init()
{
    static bool ff_osd_i2c_initialized = false;
    if (ff_osd_i2c_initialized)
        i2c_slave_deinit(I2C_INST);

    // Apply config to display (important for LCD mode)
    lcd_display_update();

    // Initialize GPIO pins for I2C
    gpio_init(I2C_SLAVE_SDA_PIN);
    gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SDA_PIN);

    gpio_init(I2C_SLAVE_SCL_PIN);
    gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SCL_PIN);

    // Initialize I2C peripheral at 100kHz
    i2c_init(I2C_INST, I2C_BAUDRATE);

    // Initialize I2C slave mode with our address and handler
    uint8_t slave_addr = settings.ff_osd_config.i2c_protocol ? 0x10 : 0x27;

    i2c_slave_init(I2C_INST, slave_addr, &i2c_slave_handler);

    // Raise I2C IRQ priority above DMA IRQ (default 0x80) so the I2C handler can
    // preempt the capture DMA handler.  Without this, a ~160µs DMA ISR blocks the
    // I2C ISR long enough that a STOP condition and the last RX byte(s) accumulate
    // simultaneously, causing phantom transactions that corrupt ff_osd_display parameters.
    irq_set_priority(I2C0_IRQ + i2c_hw_index(I2C_INST), PICO_HIGHEST_IRQ_PRIORITY);

    ff_osd_i2c_initialized = true;
}

void ff_osd_set_address()
{
    I2C_INST->hw->sar = settings.ff_osd_config.i2c_protocol ? 0x10 : 0x27;
}

void ff_osd_i2c_process(void)
{
    return settings.ff_osd_config.i2c_protocol ? ffosd_process() : lcd_process();
}

void ff_osd_update()
{
    if (!osd_state.enabled)
        return;

    if (!settings.ff_osd_config.enabled)
    {
        ff_osd_set_buttons(0);
        osd_state.visible = false;
        return;
    }

    osd_buttons_update();
    // Map OSD button presses to FF OSD button codes
    uint8_t buttons = 0;

    bool block_ff_buttons = osd_buttons_blocked();

    if (block_ff_buttons)
    {
        ff_btn_prev_held = false;
        ff_btn_pulse_frames = 0;
    }
    else if (settings.ff_osd_config.i2c_protocol)
    {
#ifdef OSD_MENU_ENABLE
        uint64_t current_time = time_us_64();

        // When local OSD menu is enabled, reserve long holds for menu activation.
        // Short taps are forwarded on release as brief host button pulses.
        bool sel_held = osd_button_held(2);
        if (!sel_held && ff_btn_prev_held)
        {
            uint64_t hold_us = current_time - osd_buttons.key_hold_start[2];
            if (hold_us < OSD_HOLD_US)
                ff_btn_pulse_frames = FF_OSD_BUTTON_PULSE_FRAMES;
        }
        ff_btn_prev_held = sel_held;

        if (ff_btn_pulse_frames > 0)
        {
            buttons |= FF_OSD_BUTTON_SELECT;
            ff_btn_pulse_frames--;
        }
#else
        // With menu disabled, do not suppress long presses in FlashFloppy mode.
        ff_btn_prev_held = false;
        ff_btn_pulse_frames = 0;

        if (osd_button_pressed(2)) // SEL -> SELECT
            buttons |= FF_OSD_BUTTON_SELECT;
#endif
    }
    else if (!settings.ff_osd_config.i2c_protocol)
    {
        // LCD mode: preserve previous immediate-press behavior.
        ff_btn_prev_held = false;
        ff_btn_pulse_frames = 0;

        if (osd_button_pressed(2)) // SEL -> SELECT
            buttons |= FF_OSD_BUTTON_SELECT;
    }

    if (!block_ff_buttons)
    {
        if (osd_button_pressed(0)) // UP -> LEFT
            buttons |= FF_OSD_BUTTON_LEFT;

        if (osd_button_pressed(1)) // DOWN -> RIGHT
            buttons |= FF_OSD_BUTTON_RIGHT;
    }

    ff_osd_set_buttons(buttons);

    if (ff_osd_display.on)
    {
        osd_font = osd_font_style_2;

        const uint8_t fg_color = 7;
        const uint8_t bg_color = 0;

        osd_mode.x = settings.ff_osd_config.h_position;
        osd_mode.y = settings.ff_osd_config.v_position ? 2 : 1; // 1 = top, 2 = bottom
        osd_mode.columns = ff_osd_display.cols;

        uint8_t double_height_rows = 0;

        for (int i = 0; i < ff_osd_display.rows; i++)
            if ((ff_osd_display.heights >> i) & 1)
                double_height_rows++;

        osd_mode.rows = ff_osd_display.rows + double_height_rows;
        osd_mode.border_enabled = false;
        osd_mode.full_width = true;
        osd_mode.width = osd_mode.columns * OSD_FONT_WIDTH;
        osd_mode.height = osd_mode.rows * OSD_FONT_HEIGHT;
        osd_mode.buffer_size = osd_mode.width * osd_mode.height / 2;

        osd_set_position();

        uint8_t start_row = 0;

        // Render each row from ff_osd_display
        for (uint8_t row = 0; (row < ff_osd_display.rows) && (row < 4); row++)
        {
            uint8_t osd_row = start_row + row;

            if (osd_row >= osd_mode.rows)
                break;

            // Check if this row should be double-height
            uint8_t is_double_height = settings.ff_osd_config.i2c_protocol && ((ff_osd_display.heights >> row) & 1);

            osd_text_heights[osd_row] = is_double_height;

            // Copy the text row and clean non-printable characters
            char row_text[41];

            uint8_t out_pos = 0;

            for (uint8_t col = 0; col < ff_osd_display.cols && col < osd_mode.columns; col++)
            {
                uint8_t glyph = (uint8_t)ff_osd_display.text[row][col];

                if (glyph < 32)
                    glyph = ' ';

                row_text[out_pos++] = (char)glyph;
            }

            row_text[out_pos] = '\0';
            // Print the entire row at once with the appropriate height
            osd_text_print(osd_row, 0, row_text, fg_color, bg_color, is_double_height);
        }
    }

    // Always mark that text buffer needs to be rendered
    osd_state.text_updated = true;
    osd_state.needs_redraw = true;

    osd_render_text_to_buffer();

    osd_state.visible = ff_osd_display.on;
}

#endif
