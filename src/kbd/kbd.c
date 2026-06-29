/**
 * kbd.c - Universal keyboard dispatcher
 *
 * Orchestrates input backends (PS/2, USB) and output backend (CH446Q or SPI)
 * through a unified keyboard state pipeline.
 */

#include "g_config.h"
#include "kbd.h"
#include "key_codes.h"
#include "led.h"
#include "zx_kbd.h"

// Hotkey for mouse button swap (override in g_config.h if needed)
#ifndef KBD_HOTKEY_MOUSE_SWAP
#define KBD_HOTKEY_MOUSE_SWAP KEY_F6
#endif
// Hotkeys for NMI and RESET signals (override in g_config.h if needed)
#ifndef KBD_HOTKEY_NMI
#define KBD_HOTKEY_NMI KEY_F11
#endif
#ifndef KBD_HOTKEY_RESET
#define KBD_HOTKEY_RESET KEY_F12
#endif

#ifndef SPI_KB_ENABLE
#include "ch446q.h"
#else
#include "epm3256.h"
#endif

#ifdef PS2_KBD_ENABLE
#include "ps2_kbd.h"
#endif

#ifdef USB_KBD_ENABLE
#include "usb_kbd.h"
#endif

#ifdef OSD_ENABLE
#include "osd_kbd.h"
#else
// When OSD is disabled, keyboard output is never suppressed
#define osd_kbd_active false
#endif

#ifdef KBD_ENABLE

volatile uint32_t kbd_activity_cnt;

static kbd_unified_state_t kbd_state;
static zx_kbd_state_t kbd_zx_state;
static zx_kbd_state_t kbd_zx_state_old;

#ifndef SPI_KB_ENABLE
static void __not_in_flash_func(kbd_apply_output)(zx_kbd_state_t *zx_new, zx_kbd_state_t *zx_old)
{
    for (int row = 0; row < 8; row++)
    {
        uint8_t changed = zx_new->a[row] ^ zx_old->a[row];

        if (changed)
            for (int col = 0; col < 5; col++)
                if (changed & (1 << col))
                {
                    bool pressed = (zx_new->a[row] & (1 << col)) != 0;
                    ch446q_set_switch(col, row, pressed);
                }
    }
}
#endif

// Default (false): right button → D0, left button → D1 (matches original schematic).
// Swapped (true):   left button → D0, right button → D1.
// F6 hotkey toggles this mapping (SPI/EPM3256 mode only).
#ifdef SPI_KB_ENABLE
static bool mouse_buttons_swapped = false;
#endif

static void __not_in_flash_func(kbd_on_event)(void)
{
    kbd_activity_cnt++;
    led_put(LED_B, (kbd_activity_cnt & 1) ? 32 : 0);

    kbd_state_t merged;

    memset(&merged, 0, sizeof(merged));

#ifdef PS2_KBD_ENABLE
    kbd_state_t *ps2 = ps2_kbd_get_state();

    for (int i = 0; i < 4; i++)
        merged.u[i] |= ps2->u[i];
#endif

#ifdef USB_KBD_ENABLE
    kbd_state_t *usb = usb_kbd_get_state();

    for (int i = 0; i < 4; i++)
        merged.u[i] |= usb->u[i];
#endif

    kbd_state.old_state = kbd_state.new_state;
    kbd_state.new_state = merged;

    // Toggle mouse button swap (edge-detect on press, SPI mode only)
#ifdef SPI_KB_ENABLE
    static bool prev_mouse_swap = false;

    bool mouse_swap = GET_STATE_KEY(kbd_state.new_state, KBD_HOTKEY_MOUSE_SWAP);
    if (mouse_swap && !prev_mouse_swap)
        mouse_buttons_swapped = !mouse_buttons_swapped;

    prev_mouse_swap = mouse_swap;
#endif

    // NMI / RESET hotkeys (level-based: active while held, released when key released)
    // CH446Q mode: F11 closes Y5:X10 (NMI), F12 closes Y6:X11 (RESET)
    // EPM3256 mode: NMI/RESET sent as bits 0/1 in every SPI frame
    static bool prev_nmi = false;
    static bool prev_reset = false;
    bool nmi = GET_STATE_KEY(kbd_state.new_state, KBD_HOTKEY_NMI);
    bool reset = GET_STATE_KEY(kbd_state.new_state, KBD_HOTKEY_RESET);

#ifndef SPI_KB_ENABLE
    if (nmi != prev_nmi)
        ch446q_set_switch(5, 10, nmi);

    if (reset != prev_reset)
        ch446q_set_switch(6, 11, reset);
#endif

    prev_nmi = nmi;
    prev_reset = reset;

#ifdef OSD_ENABLE
    osd_kbd_intercept(&kbd_state.new_state);
#endif

    if (!osd_kbd_active)
    {
        zx_kbd_set_state(&kbd_zx_state, &kbd_state.new_state);

#ifdef SPI_KB_ENABLE
        usb_mouse_state_t *mouse = usb_mouse_get_state();
        uint8_t btn = mouse->buttons;

        // Remap HID mouse buttons to Kempston bit positions.
        // Default: right → D0, left → D1 (original schematic).
        // Swapped: left  → D0, right → D1.
        uint8_t b_left = (btn >> 0) & 1;
        uint8_t b_right = (btn >> 1) & 1;

        if (mouse_buttons_swapped)
            btn = (btn & ~0x03u) | (b_left << 0) | (b_right << 1);
        else
            btn = (btn & ~0x03u) | (b_right << 0) | (b_left << 1);

        uint8_t special_keys = (nmi ? 1 : 0) | (reset ? 2 : 0);
        epm3256_send(special_keys, btn, mouse->x, mouse->y, &kbd_zx_state);
#else
        kbd_apply_output(&kbd_zx_state, &kbd_zx_state_old);
        kbd_zx_state_old = kbd_zx_state;
#endif
    }
}

void kbd_init(void)
{
    memset(&kbd_state, 0, sizeof(kbd_state));
    memset(&kbd_zx_state, 0, sizeof(kbd_zx_state));
    memset(&kbd_zx_state_old, 0, sizeof(kbd_zx_state_old));

#ifdef SPI_KB_ENABLE
    epm3256_init();
#else
    ch446q_init();
#endif

#ifdef PS2_KBD_ENABLE
    ps2_kbd_set_event_callback(kbd_on_event);
    ps2_kbd_pio_init();
#endif

#ifdef USB_KBD_ENABLE
    usb_kbd_init();
    usb_kbd_set_event_callback(kbd_on_event);
#endif
}

#endif // KBD_ENABLE
