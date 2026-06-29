/**
 * usb_kbd.c - USB HID keyboard and mouse driver using TinyUSB Host
 *
 * Converts 8-byte boot keyboard reports to universal key state.
 * Converts mouse reports to Kempston-compatible accumulated state.
 */

#include "tusb.h"
#include "class/hid/hid.h"

#include "g_config.h"
#include "usb_kbd.h"

#ifdef USB_KBD_ENABLE

// HID usage codes (0x00..0x73) → universal key codes
static const uint8_t usb_hid_to_universal[] = {
    // 0x00-0x03: reserved / error codes
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    // 0x04-0x1D: A-Z
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    // 0x1E-0x27: 1-9, 0
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    // 0x28-0x2C: Enter, Escape, Backspace, Tab, Space
    KEY_ENTER,
    KEY_ESC,
    KEY_BACK_SPACE,
    KEY_TAB,
    KEY_SPACE,
    // 0x2D-0x31: Minus, Equal, LeftBracket, RightBracket, Backslash
    KEY_MINUS,
    KEY_EQUALS,
    KEY_LEFT_BR,
    KEY_RIGHT_BR,
    KEY_BACKSLASH,
    // 0x32: Europe1 (non-US #)
    NO_KEY,
    // 0x33-0x38: Semicolon, Apostrophe, Grave, Comma, Period, Slash
    KEY_SEMICOLON,
    KEY_QUOTE,
    KEY_TILDE,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,
    // 0x39: Caps Lock
    KEY_CAPS_LOCK,
    // 0x3A-0x45: F1-F12
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    // 0x46-0x48: PrintScreen, ScrollLock, Pause
    KEY_PRT_SCR,
    KEY_SCROLL_LOCK,
    KEY_PAUSE_BREAK,
    // 0x49-0x4E: Insert, Home, PageUp, Delete, End, PageDown
    KEY_INSERT,
    KEY_HOME,
    KEY_PAGE_UP,
    KEY_DELETE,
    KEY_END,
    KEY_PAGE_DOWN,
    // 0x4F-0x52: Right, Left, Down, Up
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    // 0x53: Num Lock
    KEY_NUM_LOCK,
    // 0x54-0x63: Keypad /, *, -, +, Enter, 1-9, 0, .
    KEY_NUM_SLASH,
    KEY_NUM_MULT,
    KEY_NUM_MINUS,
    KEY_NUM_PLUS,
    KEY_NUM_ENTER,
    KEY_NUM_1,
    KEY_NUM_2,
    KEY_NUM_3,
    KEY_NUM_4,
    KEY_NUM_5,
    KEY_NUM_6,
    KEY_NUM_7,
    KEY_NUM_8,
    KEY_NUM_9,
    KEY_NUM_0,
    KEY_NUM_PERIOD,
    // 0x64: Europe2
    NO_KEY,
    // 0x65: Application/Menu
    KEY_MENU,
    // 0x66-0x73: Power and beyond (not used)
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
    NO_KEY,
};

#define USB_HID_TO_UNIVERSAL_SIZE (sizeof(usb_hid_to_universal) / sizeof(usb_hid_to_universal[0]))

static kbd_state_t usb_kbd_state;
static usb_mouse_state_t usb_mouse_state;
static usb_kbd_event_fn usb_kbd_event_cb = NULL;
static uint8_t usb_dev_addr = 0;
static uint8_t usb_hid_idx = 0;
static bool usb_keyboard_mounted = false;
static uint8_t usb_mouse_dev_addr = 0;
static uint8_t usb_mouse_hid_idx = 0;
static bool usb_mouse_mounted = false;

static void process_kbd_report(uint8_t const *report, uint16_t len)
{
    if (len < 8)
        return;

    // Boot keyboard report: [modifier, reserved, key1..key6]
    uint8_t modifier = report[0];

    // Build new state from scratch
    kbd_state_t new_state;

    memset(&new_state, 0, sizeof(new_state));

    // Map modifier bits to universal keys
    if (modifier & KEYBOARD_MODIFIER_LEFTCTRL)
        SET_STATE_KEY(new_state, KEY_L_CTRL);

    if (modifier & KEYBOARD_MODIFIER_LEFTSHIFT)
        SET_STATE_KEY(new_state, KEY_L_SHIFT);

    if (modifier & KEYBOARD_MODIFIER_LEFTALT)
        SET_STATE_KEY(new_state, KEY_L_ALT);

    if (modifier & KEYBOARD_MODIFIER_LEFTGUI)
        SET_STATE_KEY(new_state, KEY_L_WIN);

    if (modifier & KEYBOARD_MODIFIER_RIGHTCTRL)
        SET_STATE_KEY(new_state, KEY_R_CTRL);

    if (modifier & KEYBOARD_MODIFIER_RIGHTSHIFT)
        SET_STATE_KEY(new_state, KEY_R_SHIFT);

    if (modifier & KEYBOARD_MODIFIER_RIGHTALT)
        SET_STATE_KEY(new_state, KEY_R_ALT);

    if (modifier & KEYBOARD_MODIFIER_RIGHTGUI)
        SET_STATE_KEY(new_state, KEY_R_WIN);

    // Map key array (up to 6 simultaneous keys)
    for (int i = 2; i < 8; i++)
    {
        uint8_t hid_code = report[i];

        if (hid_code == 0 || hid_code >= USB_HID_TO_UNIVERSAL_SIZE)
            continue;

        uint8_t ucode = usb_hid_to_universal[hid_code];

        if (ucode != NO_KEY)
            SET_STATE_KEY(new_state, ucode);
    }

    // Check if state actually changed
    if (memcmp(&usb_kbd_state, &new_state, sizeof(kbd_state_t)) == 0)
        return;

    usb_kbd_state = new_state;

    if (usb_kbd_event_cb)
        usb_kbd_event_cb();
}

static void __not_in_flash_func(process_mouse_report)(uint8_t const *report, uint16_t len)
{
    // Boot mouse: [buttons, dx, dy] (3 bytes)
    // Boot mouse with wheel: [buttons, dx, dy, wheel] (4 bytes)
    // Some mice send [report_id, buttons, dx, dy, ...] (5+ bytes)
    int d = 0;

    if (len > 4)
        d = 1;

    if (len < (3 + d))
        return;

    uint8_t buttons = report[0 + d];
    int8_t dx = (int8_t)report[1 + d];
    int8_t dy = (int8_t)report[2 + d];

    // TODO: wheel support (use later)
    // if (len >= (4 + d))
    // {
    //     int8_t dw = (int8_t)report[3 + d];
    //     usb_mouse_state.has_wheel = true;
    //     usb_mouse_state.wheel = (usb_mouse_state.wheel + dw) & 0x0F;
    // }

    // Accumulate position (Kempston mouse wraps 0-255)
    int x = usb_mouse_state.x + dx;
    int y = usb_mouse_state.y - dy; // Y inverted for Kempston

    if (x > 255)
        x &= 255;

    if (x < 0)
        x += 256;

    if (y > 255)
        y &= 255;

    if (y < 0)
        y += 256;

    // Build buttons byte: bits 0-2 inverted buttons, bit 3 always 1,
    // bits 4-7: 0xF (no wheel support yet)
    uint8_t btn = (buttons ^ 0xFF) & 0x07;
    btn |= 0x08; // bit 3 always 1
    btn |= 0xF0; // bits 4-7 = 1111 (no wheel)

    // TODO: wheel support (use later)
    // if (usb_mouse_state.has_wheel)
    //     btn |= (usb_mouse_state.wheel << 4);
    // else
    //     btn |= 0xF0;

    usb_mouse_state.buttons = btn;
    usb_mouse_state.x = (uint8_t)x;
    usb_mouse_state.y = (uint8_t)y;

    if (usb_kbd_event_cb)
        usb_kbd_event_cb();
}

// TinyUSB Host HID Callbacks

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *desc_report, uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;

    uint8_t const protocol = tuh_hid_interface_protocol(dev_addr, idx);

    if (protocol == HID_ITF_PROTOCOL_KEYBOARD)
    {
        usb_dev_addr = dev_addr;
        usb_hid_idx = idx;
        usb_keyboard_mounted = true;

        // Use boot protocol for simplicity
        if (tuh_hid_get_protocol(dev_addr, idx) != HID_PROTOCOL_BOOT)
            tuh_hid_set_protocol(dev_addr, idx, HID_PROTOCOL_BOOT);

        // Start receiving reports
        tuh_hid_receive_report(dev_addr, idx);
    }
    else if (protocol == HID_ITF_PROTOCOL_MOUSE)
    {
        usb_mouse_dev_addr = dev_addr;
        usb_mouse_hid_idx = idx;
        usb_mouse_mounted = true;

        if (tuh_hid_get_protocol(dev_addr, idx) != HID_PROTOCOL_BOOT)
            tuh_hid_set_protocol(dev_addr, idx, HID_PROTOCOL_BOOT);

        tuh_hid_receive_report(dev_addr, idx);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx)
{
    if (dev_addr == usb_dev_addr && idx == usb_hid_idx)
    {
        usb_keyboard_mounted = false;
        usb_dev_addr = 0;
        usb_hid_idx = 0;
        memset(&usb_kbd_state, 0, sizeof(usb_kbd_state));

        if (usb_kbd_event_cb)
            usb_kbd_event_cb();
    }
    else if (dev_addr == usb_mouse_dev_addr && idx == usb_mouse_hid_idx)
    {
        usb_mouse_mounted = false;
        usb_mouse_dev_addr = 0;
        usb_mouse_hid_idx = 0;
        memset(&usb_mouse_state, 0, sizeof(usb_mouse_state));
        usb_mouse_state.buttons = 0xFF; // all buttons released (active-low)

        if (usb_kbd_event_cb)
            usb_kbd_event_cb();
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx,
                                uint8_t const *report, uint16_t len)
{
    if (dev_addr == usb_dev_addr && idx == usb_hid_idx)
        process_kbd_report(report, len);
    else if (dev_addr == usb_mouse_dev_addr && idx == usb_mouse_hid_idx)
        process_mouse_report(report, len);

    // Continue receiving
    tuh_hid_receive_report(dev_addr, idx);
}

// Public API

void usb_kbd_init(void)
{
    memset(&usb_kbd_state, 0, sizeof(usb_kbd_state));
    memset(&usb_mouse_state, 0, sizeof(usb_mouse_state));
    usb_mouse_state.buttons = 0xFF; // all buttons released (active-low)
    usb_keyboard_mounted = false;
    usb_dev_addr = 0;
    usb_hid_idx = 0;
    usb_mouse_mounted = false;
    usb_mouse_dev_addr = 0;
    usb_mouse_hid_idx = 0;
}

void usb_kbd_task(void)
{
    tuh_task();
}

void usb_kbd_set_event_callback(usb_kbd_event_fn cb)
{
    usb_kbd_event_cb = cb;
}

kbd_state_t *usb_kbd_get_state(void)
{
    return &usb_kbd_state;
}

usb_mouse_state_t *usb_mouse_get_state(void)
{
    return &usb_mouse_state;
}

#endif // USB_KBD_ENABLE
