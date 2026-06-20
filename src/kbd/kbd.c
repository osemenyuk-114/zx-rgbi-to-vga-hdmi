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

static bool mouse_buttons_swapped = false;

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

    // F6: toggle mouse button swap (edge-detect on press)
    {
        static bool prev_f6 = false;
        bool f6 = GET_STATE_KEY(kbd_state.new_state, KEY_F6);
        if (f6 && !prev_f6)
            mouse_buttons_swapped = !mouse_buttons_swapped;
        prev_f6 = f6;
    }

#ifdef OSD_ENABLE
    osd_kbd_intercept(&kbd_state.new_state);
#endif

    if (!osd_kbd_active)
    {
        zx_kbd_set_state(&kbd_zx_state, &kbd_state.new_state);

#ifdef SPI_KB_ENABLE
        usb_mouse_state_t *mouse = usb_mouse_get_state();
        uint8_t btn = mouse->buttons;
        if (mouse_buttons_swapped)
        {
            // Swap bits 0 (left) and 1 (right) in Kempston-encoded buttons
            uint8_t b0 = (btn >> 0) & 1;
            uint8_t b1 = (btn >> 1) & 1;
            btn = (btn & ~0x03u) | (b0 << 1) | (b1 << 0);
        }
        epm3256_send(btn, mouse->x, mouse->y, &kbd_zx_state);
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
