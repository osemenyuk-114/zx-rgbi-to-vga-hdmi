/*
 *
 * I2C communications to the host:
 *  1. Emulate HD44780 LCD controller via a PCF8574 I2C backpack.
 *  2. Support extended custom protocol with bidirectional comms.
 *
  Based on https://github.com/keirf/flashfloppy-osd  by Keir Fraser <keir.xen@gmail.com>
 *
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

/*
  GOTEK floppy drive emulator with flashfloppy firmware I2C LCD OSD interface.

  Simultaneous OLED/LCD screen and on-screen display can be supported with FF OSD protocol build enabled.
  With single FF OSD 40 text columns and up to 4 rows are displayed.
  With dual displays FF OSD have same text columns count as primary OLED/LCD.

  GOTEK configuration:
  https://github.com/keirf/flashfloppy/wiki/Hardware-Mods#lcd-display
  https://github.com/keirf/flashfloppy/wiki/FF.CFG-Configuration-File

  FF.CFG:

  for FF OSD protocol with dual OLED/LCD support:
  set:
  display-type=auto
  or
  display-type = oled-128x64
  with
  osd-display-order = 3,0
  osd-columns = 40
  and
  display-off-secs = 0-255 (60 by default)

  display-order and osd-display-order can be set independently.

  for single PCF8574 20x4 LCD display protocol:
  set:
  display-type=lcd-20x04
  with
  display-order=3,0,2,1
  and
  display-off-secs = 0-255 (60 by default)

  Wiring:
  I2C uses 2 wires: SDA and SCL to connect RGBI2VGA adapter and GOTEK.
  Connect GOTEK SDA and SCL pins to free Pico I2C pins pair: see I2C_SLAVE_SDA_PIN, I2C_SLAVE_SCL_PIN constants declaration.
  SDA and SCL lines should be pulled up to VCC(3.3V) with 4.7~10K resistors on GOTEK or RGBI2VGA side.

  Russian filenames are supported, requires https://github.com/proboterror/flashfloppy-russian patched flashfloppy GOTEK firmware.

  Build:
  Set FF_OSD_SUPPORT to 0 to disable custom FF OSD protocol support.
*/

#include <memory.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "i2c_fifo.h"
#include "i2c_slave.h"

#include "g_config.h"
#include "ff_osd.h"
#include "osd.h"
#include "video_output.h"

// Helper macros
#define min_t(type, x, y) \
    ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })

#define max_t(type, x, y) \
    ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MASK(r, x) ((x) & (ARRAY_SIZE(r) - 1))

#define barrier() asm volatile("" ::: "memory")

#define FF_OSD_FW_VER "1.9"

// Use GP18/19 (I2C1), available on both WAVESHARE RP2040 ZERO and full size Raspberry Pi Pico boards.
static const uint I2C_SLAVE_SDA_PIN = 18;
static const uint I2C_SLAVE_SCL_PIN = 19;
#define I2C_INST i2c1

// 100 kHz: I2C Standard Mode, matching flashfloppy setting
#define I2C_BAUDRATE 100000

// PCF8574 pin assignment: D7-D6-D5-D4-BL-EN-RW-RS
#define _D7 (1u << 7)
#define _D6 (1u << 6)
#define _D5 (1u << 5)
#define _D4 (1u << 4)
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

#define FF_OSD_COLUMNS_MIN 16
#define FF_OSD_COLUMNS_MAX 40

const char ff_osd_fw_ver[] = FF_OSD_FW_VER;

extern settings_t settings;

const static ff_osd_config_t default_ff_osd_config = {
    .i2c_protocol = false,
    .cols = 16,
    .rows = 2,
};

ff_osd_config_t ff_osd_config = {
    .i2c_protocol = true,
    .cols = 40,
    .rows = 4,
};

bool config_active;

static enum {
    C_idle = 0,
    C_banner,
    // LCD
    C_rows,
    C_cols,
    // Exit
    C_save,
    C_max
} config_state;

// Display state, exported to display routines
ff_osd_display_t ff_osd_display = {
    .rows = 4,
    .cols = 20,
    .heights = 0,
    .on = false,
    .text = {},
};

uint8_t ff_osd_buttons_rx; // button state: Gotek -> OSD

// state: OSD -> Gotek
ff_osd_info_t ff_osd_info = {
    .protocol_ver = 0,
    .fw_major = ff_osd_fw_ver[0],
    .fw_minor = ff_osd_fw_ver[2],
    .buttons = 0};

// Current position in FF OSD I2C Protocol character data.
static uint8_t ff_osd_x, ff_osd_y;

// I2C data ring
static uint8_t d_ring[1024];
static uint16_t d_cons, d_prod; // data ring buffer consumer / producer pointers

// Transaction ring: Data-ring offset of each transaction start
static uint16_t t_ring[8];
static uint16_t t_cons, t_prod; // transactions ring buffer consumer / producer pointers

// LCD state
static bool lcd_inc;
static uint8_t lcd_ddraddr;

static uint16_t display_height;

static struct repeat
{
    int repeat;
    uint32_t prev;
} left, right;

uint8_t button_repeat(uint8_t pb, uint8_t b, uint8_t m, struct repeat *r)
{
    if (pb & m)
    {
        // Is this button held down?
        if (b & m)
        {
            uint32_t delta = r->repeat ? 100000 : 500000;

            if (time_us_32() - r->prev > delta)
            {
                // Repeat this button now.
                r->repeat++;
            }
            else
            {
                // Not ready to repeat this button.
                b &= ~m;
            }
        }
        else
        {
            // Button not pressed. Reset repeat count.
            r->repeat = 0;
        }
    }

    if (b & m)
    {
        // Remember when we actioned this button press/repeat.
        r->prev = time_us_32();
    }

    return b;
}

uint16_t ff_osd_set_cols(int16_t cols)
{
    return min_t(uint16_t, max_t(uint16_t, cols, FF_OSD_COLUMNS_MIN), FF_OSD_COLUMNS_MAX);
}

void ff_osd_set_buttons(uint8_t buttons)
{
    ff_osd_info.buttons = buttons;
}

// Called before erasing Flash, to temporarily disable the display.
// Flash updates can stall instruction fetch and mess up the OSD.
void display_off(void)
{
    display_height = 0; // Display off
    sleep_ms(50);       // Wait for a few hlines (we only really need one)
}

static void lcd_display_update(void)
{
    if (ff_osd_config.i2c_protocol)
        return;

    ff_osd_display.rows = ff_osd_config.rows;
    ff_osd_display.cols = ff_osd_config.cols;
}

void __not_in_flash_func(i2c_slave_handler)(i2c_inst_t *i2c, i2c_slave_event_t event)
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
    ff_osd_config = settings.ff_osd_config;

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
    uint8_t slave_addr = ff_osd_config.i2c_protocol ? 0x10 : 0x27;
    i2c_slave_init(I2C_INST, slave_addr, &i2c_slave_handler);
}

void ff_osd_set_address()
{
    uint8_t slave_addr = ff_osd_config.i2c_protocol ? 0x10 : 0x27;
    I2C_INST->hw->sar = slave_addr;
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
        ff_osd_y = 0;
    }

    // Data ring should not be more than half full. We don't want it to
    // overrun during the processing loop below: That should be impossible
    // with half a ring free.
    // assert((uint16_t)(d_p - d_c) < (ARRAY_SIZE(d_ring)/2));

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
                ff_osd_display.cols = min_t(uint16_t, 40, x & 0x3f);
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
        ff_osd_display.cols = min_t(unsigned int, x + 1, FF_OSD_COLUMNS_MAX);
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

void ff_osd_i2c_process(void)
{
    return ff_osd_config.i2c_protocol ? ffosd_process() : lcd_process();
}

void ff_osd_update()
{
    if (!osd_state.enabled)
        return;

    osd_buttons_update();
    // Map OSD button presses to FF OSD button codes
    uint8_t buttons = 0;

    if (osd_button_pressed(0)) // UP -> LEFT
        buttons |= FF_OSD_BUTTON_LEFT;

    if (osd_button_pressed(1)) // DOWN -> RIGHT
        buttons |= FF_OSD_BUTTON_RIGHT;

    if (osd_button_pressed(2)) // SEL -> SELECT
        buttons |= FF_OSD_BUTTON_SELECT;

    ff_osd_set_buttons(buttons);

    if (ff_osd_display.on)
    {

        const uint8_t fg_color = 7;
        const uint8_t bg_color = 0;

        osd_mode.x = 1;
        osd_mode.y = 1;
        osd_mode.columns = ff_osd_display.cols;

        uint8_t double_height_rows = 0;

        for (int i = 0; i < ff_osd_display.rows; i++)
            if ((ff_osd_display.heights >> i) & 1)
                double_height_rows++;

        osd_mode.rows = ff_osd_display.rows + double_height_rows;
        osd_mode.border_enabled = false;
        osd_mode.width = osd_mode.columns * OSD_FONT_WIDTH;
        osd_mode.height = osd_mode.rows * OSD_FONT_HEIGHT;
        osd_mode.buffer_size = osd_mode.width * osd_mode.height / 2;

        osd_set_position();

        ff_osd_config_process(ff_osd_buttons_rx | (config_active ? buttons : 0), fg_color, bg_color);

        uint8_t start_row = 0;

        // Render each row from ff_osd_display
        for (uint8_t row = 0; (row < ff_osd_display.rows) && (row < 4); row++)
        {
            uint8_t osd_row = start_row + row;

            if (osd_row >= osd_mode.rows)
                break;

            if (config_active && (row < 2))
                continue;

            // Check if this row should be double-height
            uint8_t is_double_height = ff_osd_config.i2c_protocol && ((ff_osd_display.heights >> row) & 1);

            osd_text_heights[osd_row] = is_double_height;

            // Calculate starting column to center the text horizontally
            uint8_t text_len = strnlen(ff_osd_display.text[row], ff_osd_display.cols);

            if (text_len == 0)
                continue; // Skip empty rows

            // Use bright colors for double-height text, normal colors otherwise
            // uint8_t fg_color = is_double_height ? OSD_COLOR_TEXT : OSD_COLOR_DIMMED;
            // uint8_t fg_color = 7;
            // Copy the text row and clean non-printable characters
            char row_text[41];

            uint8_t out_pos = 0;

            for (uint8_t col = 0; col < text_len && col < ff_osd_display.cols && col < osd_mode.columns; col++)
            {
                char c = ff_osd_display.text[row][col];
                // Skip non-printable characters
                if (c < 32 || c > 126)
                    c = ' ';
                row_text[out_pos++] = c;
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

void ff_osd_config_process(uint8_t b, uint8_t fg_color, uint8_t bg_color)
{
    uint8_t _b;
    static uint8_t pb;
    bool changed = false;

    static enum {
        C_SAVE = 0,
        C_SAVEREBOOT,
        C_USE,
        C_DISCARD,
        C_RESET,
        C_NC_MAX,
    } new_config;

    static ff_osd_config_t old_config;

    _b = b;
    b &= b ^ (pb & FF_OSD_BUTTON_SELECT);
    b = button_repeat(pb, b, FF_OSD_BUTTON_LEFT, &left);
    b = button_repeat(pb, b, FF_OSD_BUTTON_RIGHT, &right);
    pb = _b;

    if (b & FF_OSD_BUTTON_SELECT)
    {
        if (++config_state >= C_max)
        {
            config_state = C_idle;

            display_off();

            switch (new_config)
            {
            case C_SAVE:
                // config_write_flash(&ff_osd_config);
                break;

            case C_SAVEREBOOT:
                /*
                config_write_flash(&ff_osd_config);

                while (1)
                {
                } // hang and let WDT reboot
                */
                break;

            case C_USE:
                break;

            case C_DISCARD:
                ff_osd_config = old_config;
                break;

            case C_RESET:
                ff_osd_config = default_ff_osd_config;
                /*
                config_write_flash(&ff_osd_config);

                while (1)
                {
                } // hang and let WDT reboot
                */
                break;

            case C_NC_MAX:
                break;
            }

            lcd_display_update();
        }

        if ((config_state == C_rows) && ff_osd_config.i2c_protocol)
        {
            // Skip LCD config options if using the extended OSD protocol.
            config_state = C_save;
        }

        config_active = (config_state != C_idle);
        changed = true;
    }

    switch (config_state)
    {
    case C_banner:
        if (changed)
        {
            osd_text_printf(0, 0, fg_color, bg_color, 0, "FF OSD v%s", ff_osd_fw_ver);
            osd_text_print(1, 0, "Flash Config", fg_color, bg_color, 0);

            old_config = ff_osd_config;
        }

        break;

    case C_rows:
        if (changed)
            osd_text_print(0, 0, "Rows (2 or 4):", fg_color, bg_color, 0);

        if (b & (FF_OSD_BUTTON_LEFT | FF_OSD_BUTTON_RIGHT))
            ff_osd_config.rows = (ff_osd_config.rows == 2) ? 4 : 2;

        if (b)
            osd_text_printf(1, 0, fg_color, bg_color, 0, "%u", ff_osd_config.rows);

        break;

    case C_cols:
        if (changed)
            osd_text_printf(0, 0, fg_color, bg_color, 0, "Col (%u-%u):", FF_OSD_COLUMNS_MIN, FF_OSD_COLUMNS_MAX);

        if (b & FF_OSD_BUTTON_LEFT)
            ff_osd_config.cols--;

        if (b & FF_OSD_BUTTON_RIGHT)
            ff_osd_config.cols++;

        ff_osd_config.cols = min_t(uint16_t, max_t(uint16_t, ff_osd_config.cols, FF_OSD_COLUMNS_MIN), FF_OSD_COLUMNS_MAX);

        if (b)
            osd_text_printf(1, 0, fg_color, bg_color, 0, "%u", ff_osd_config.cols);

        break;

    case C_save:
    {
        const static char *str[] = {
            "Save",
            "Save+Reset",
            "Use",
            "Discard",
            "Factory Reset",
        };

        if (changed)
        {
            osd_text_print(0, 0, "Save New config?", fg_color, bg_color, 0);
            new_config = C_SAVE;
        }

        if (b & FF_OSD_BUTTON_LEFT)
        {
            if (new_config > 0)
                --new_config;
            else
                new_config = C_NC_MAX - 1;
        }

        if (b & FF_OSD_BUTTON_RIGHT)
            if (++new_config >= C_NC_MAX)
                new_config = 0;

        if (b)
            osd_text_printf(1, 0, fg_color, bg_color, 0, "%s", str[new_config]);

        break;
    }

    default:
        break;
    }
}