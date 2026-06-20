/**
 * ps2_kbd.c - PIO-based PS/2 keyboard driver
 *
 * IRQ-driven: PIO RX FIFO not-empty interrupt triggers immediate
 * scancode decoding and callback invocation.
 */

#include "hardware/clocks.h"
#include "hardware/irq.h"

#include "g_config.h"
#include "ps2_kbd.h"
#include "kbd.pio.h"

static kbd_state_t ps2_kbd_state;
static ps2_kbd_event_fn ps2_kbd_event_cb = NULL;
static const uint8_t ps2_scancode_std_lut[256] = {
    [0x01] = KEY_F9,
    [0x03] = KEY_F5,
    [0x04] = KEY_F3,
    [0x05] = KEY_F1,
    [0x06] = KEY_F2,
    [0x07] = KEY_F12,
    [0x09] = KEY_F10,
    [0x0A] = KEY_F8,
    [0x0B] = KEY_F6,
    [0x0C] = KEY_F4,
    [0x0D] = KEY_TAB,
    [0x0E] = KEY_TILDE,
    [0x11] = KEY_L_ALT,
    [0x12] = KEY_L_SHIFT,
    [0x14] = KEY_L_CTRL,
    [0x15] = KEY_Q,
    [0x16] = KEY_1,
    [0x1A] = KEY_Z,
    [0x1B] = KEY_S,
    [0x1C] = KEY_A,
    [0x1D] = KEY_W,
    [0x1E] = KEY_2,
    [0x21] = KEY_C,
    [0x22] = KEY_X,
    [0x23] = KEY_D,
    [0x24] = KEY_E,
    [0x25] = KEY_4,
    [0x26] = KEY_3,
    [0x29] = KEY_SPACE,
    [0x2A] = KEY_V,
    [0x2B] = KEY_F,
    [0x2C] = KEY_T,
    [0x2D] = KEY_R,
    [0x2E] = KEY_5,
    [0x31] = KEY_N,
    [0x32] = KEY_B,
    [0x33] = KEY_H,
    [0x34] = KEY_G,
    [0x35] = KEY_Y,
    [0x36] = KEY_6,
    [0x3A] = KEY_M,
    [0x3B] = KEY_J,
    [0x3C] = KEY_U,
    [0x3D] = KEY_7,
    [0x3E] = KEY_8,
    [0x41] = KEY_COMMA,
    [0x42] = KEY_K,
    [0x43] = KEY_I,
    [0x44] = KEY_O,
    [0x45] = KEY_0,
    [0x46] = KEY_9,
    [0x49] = KEY_PERIOD,
    [0x4A] = KEY_SLASH,
    [0x4B] = KEY_L,
    [0x4C] = KEY_SEMICOLON,
    [0x4D] = KEY_P,
    [0x4E] = KEY_MINUS,
    [0x52] = KEY_QUOTE,
    [0x54] = KEY_LEFT_BR,
    [0x55] = KEY_EQUALS,
    [0x58] = KEY_CAPS_LOCK,
    [0x59] = KEY_R_SHIFT,
    [0x5A] = KEY_ENTER,
    [0x5B] = KEY_RIGHT_BR,
    [0x5D] = KEY_BACKSLASH,
    [0x66] = KEY_BACK_SPACE,
    [0x69] = KEY_NUM_1,
    [0x6B] = KEY_NUM_4,
    [0x6C] = KEY_NUM_7,
    [0x70] = KEY_NUM_0,
    [0x71] = KEY_NUM_PERIOD,
    [0x72] = KEY_NUM_2,
    [0x73] = KEY_NUM_5,
    [0x74] = KEY_NUM_6,
    [0x75] = KEY_NUM_8,
    [0x76] = KEY_ESC,
    [0x77] = KEY_NUM_LOCK,
    [0x78] = KEY_F11,
    [0x79] = KEY_NUM_PLUS,
    [0x7A] = KEY_NUM_3,
    [0x7B] = KEY_NUM_MINUS,
    [0x7C] = KEY_NUM_MULT,
    [0x7D] = KEY_NUM_9,
    [0x7E] = KEY_SCROLL_LOCK,
    [0x83] = KEY_F7,
    // All other entries default to NO_KEY (0)
};

// Extended PS/2 Set 2 scancodes (E0 prefix)
static const uint8_t ps2_scancode_e0_lut[256] = {
    [0x11] = KEY_R_ALT,
    [0x12] = KEY_PRT_SCR,
    [0x14] = KEY_R_CTRL,
    [0x1F] = KEY_L_WIN,
    [0x27] = KEY_R_WIN,
    [0x2F] = KEY_MENU,
    [0x4A] = KEY_NUM_SLASH,
    [0x5A] = KEY_NUM_ENTER,
    [0x69] = KEY_END,
    [0x6B] = KEY_LEFT,
    [0x6C] = KEY_HOME,
    [0x70] = KEY_INSERT,
    [0x71] = KEY_DELETE,
    [0x72] = KEY_DOWN,
    [0x74] = KEY_RIGHT,
    [0x75] = KEY_UP,
    [0x7A] = KEY_PAGE_DOWN,
    [0x7D] = KEY_PAGE_UP,
    // All other entries default to NO_KEY (0)
};

static inline bool parity8(uint8_t data)
{
    return __builtin_popcount(data) & 1;
}

static void gpio_init_ps2(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_disable_pulls(gpio);
}

static uint8_t __not_in_flash_func(ps2_get_scancode)(void)
{
    if (pio_sm_is_rx_fifo_empty(PIO_PS2, SM_PS2))
        return 0;

    uint32_t val = pio_sm_get(PIO_PS2, SM_PS2);

    // Extract 11 bits (shift right by 21 to align)
    val >>= 21;

    // Validate frame: start bit=0, stop bit=1, odd parity correct
    // Odd parity: parity_bit should be opposite of parity8(data)
    // Error when parity_bit == parity8(data) (both same means even parity)
    if (((val & 0x401) != 0x400) ||
        ((val >> 9) & 1) == parity8((val >> 1) & 0xFF))
    {
        // PS/2 frame error — restart state machine
        pio_sm_restart(PIO_PS2, SM_PS2);
        memset(&ps2_kbd_state, 0, sizeof(ps2_kbd_state));
        return 0;
    }

    // Return 8-bit scancode
    return (val >> 1) & 0xFF;
}

static void __not_in_flash_func(translate_scancode)(uint8_t code, bool is_press, bool is_e0, bool is_e1)
{
    uint8_t key_code;

    // Special handling for PAUSE/BREAK (E1 prefix)
    if (is_e1)
        key_code = (code == 0x14) ? KEY_PAUSE_BREAK : NO_KEY;
    // Extended keys (E0 prefix)
    else if (is_e0)
        key_code = ps2_scancode_e0_lut[code];
    // Standard keys (no E0 prefix)
    else
        key_code = ps2_scancode_std_lut[code];

    // Update state if valid key code
    if (key_code != NO_KEY)
    {
        if (is_press)
            SET_STATE_KEY(ps2_kbd_state, key_code);
        else
            CLR_STATE_KEY(ps2_kbd_state, key_code);
    }
}

static bool __not_in_flash_func(ps2_decode_scancode)(void)
{
    static bool is_e0 = false;
    static bool is_e1 = false;
    static bool is_f0 = false;

    uint8_t scancode = ps2_get_scancode();

    if (scancode == 0xE0)
    {
        is_e0 = true;
        return false;
    }

    if (scancode == 0xE1)
    {
        is_e1 = true;
        return false;
    }

    if (scancode == 0xF0)
    {
        is_f0 = true;
        return false;
    }

    if (scancode)
    {
        translate_scancode(scancode, !is_f0, is_e0, is_e1);
        is_e0 = false;

        if (is_f0)
            is_e1 = false;

        is_f0 = false;
        return true;
    }

    return false;
}

static void __not_in_flash_func(ps2_pio_irq_handler)(void)
{
    while (ps2_decode_scancode())
        if (ps2_kbd_event_cb)
            ps2_kbd_event_cb();
}

void ps2_kbd_set_event_callback(ps2_kbd_event_fn cb)
{
    ps2_kbd_event_cb = cb;
}

kbd_state_t *ps2_kbd_get_state(void)
{
    return &ps2_kbd_state;
}

void ps2_kbd_clear_state(void)
{
    memset(&ps2_kbd_state, 0, sizeof(ps2_kbd_state));
}

void ps2_kbd_pio_init(void)
{
    // Add PIO program at offset 0 (static — never unloaded, first in PIO1 memory)
    uint offset = 0;
    pio_add_program_at_offset(PIO_PS2, &pio_ps2_program, offset);

    // Patch wait instructions with the actual PS2_PIN_CLK from g_config.h
    // (the .pio file uses a placeholder value)
    PIO_PS2->instr_mem[offset + pio_ps2_offset_wait_clk_low] = pio_encode_wait_gpio(false, PS2_PIN_CLK);
    PIO_PS2->instr_mem[offset + pio_ps2_offset_wait_clk_high] = pio_encode_wait_gpio(true, PS2_PIN_CLK);

    // Configure state machine
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + (pio_ps2_program.length - 1));
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_in_shift(&c, true, true, 11); // Auto-push after 11 bits
    sm_config_set_in_pins(&c, PS2_PIN_DATA);

    // Initialize GPIO pins
    gpio_init_ps2(PS2_PIN_CLK);
    gpio_init_ps2(PS2_PIN_DATA);

    // Initialize and start state machine
    pio_sm_init(PIO_PS2, SM_PS2, offset, &c);
    pio_sm_set_enabled(PIO_PS2, SM_PS2, true);

    // Set state machine clock to 200 kHz for reliable PS/2 sampling
    float fdiv = (float)clock_get_hz(clk_sys) / 200000.0f;
    pio_sm_set_clkdiv(PIO_PS2, SM_PS2, fdiv);

    // Enable PIO IRQ for RX FIFO not empty — immediate scancode processing
    pio_set_irqn_source_enabled(PIO_PS2, 1,
                                pis_sm0_rx_fifo_not_empty + SM_PS2, true);
    irq_set_exclusive_handler(PIO_PS2_IRQ, ps2_pio_irq_handler);
    irq_set_enabled(PIO_PS2_IRQ, true);
}
