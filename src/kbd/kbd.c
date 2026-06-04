/**
 * kbd.c - Universal keyboard dispatcher
 *
 * Orchestrates input backends (PS/2, USB) and output backend (CH446Q)
 * through a unified keyboard state pipeline.
 */

#include "kbd.h"

#ifdef KBD_ENABLE

#include <string.h>
#include "kbd_zx.h"

#ifdef PS2_KBD_ENABLE
#include "kbd_ps2.h"
#endif

#ifdef USB_KBD_ENABLE
#include "kbd_usb.h"
#endif

#include "kbd_ch446q.h"

#ifdef OSD_ENABLE
#include "kbd_osd.h"
#endif

static kb_u_state_t kbd_state;
static zx_kb_state_t kbd_zx_state;
static zx_kb_state_t kbd_zx_state_old;

static void __not_in_flash_func(ch446q_output_apply)(zx_kb_state_t *zx_new, zx_kb_state_t *zx_old)
{
    for (int row = 0; row < 8; row++)
    {
        uint8_t changed = zx_new->a[row] ^ zx_old->a[row];
        if (changed)
        {
            for (int col = 0; col < 5; col++)
            {
                if (changed & (1 << col))
                {
                    bool pressed = (zx_new->a[row] & (1 << col)) != 0;
                    ch446q_set_switch(col, row, pressed);
                }
            }
        }
    }
}

static void __not_in_flash_func(kbd_on_event)(void)
{
    kb_state_t merged;
    memset(&merged, 0, sizeof(merged));

#ifdef PS2_KBD_ENABLE
    {
        kb_state_t *ps2 = ps2_get_kb_state();
        for (int i = 0; i < 4; i++)
            merged.u[i] |= ps2->u[i];
    }
#endif

#ifdef USB_KBD_ENABLE
    {
        kb_state_t *usb = kbd_usb_get_kb_state();
        for (int i = 0; i < 4; i++)
            merged.u[i] |= usb->u[i];
    }
#endif

    kbd_state.old_state = kbd_state.new_state;
      kbd_state.new_state = merged;

#ifdef OSD_ENABLE
    kbd_osd_intercept(&kbd_state.new_state);
#endif

    if (!kbd_osd_active)
    {
        set_zx_kb_state(&kbd_zx_state, &kbd_state.new_state);
        ch446q_output_apply(&kbd_zx_state, &kbd_zx_state_old);
        kbd_zx_state_old = kbd_zx_state;
    }
}

void kbd_init(void)
{
    memset(&kbd_state, 0, sizeof(kbd_state));
    memset(&kbd_zx_state, 0, sizeof(kbd_zx_state));
    memset(&kbd_zx_state_old, 0, sizeof(kbd_zx_state_old));

    ch446q_init();

#ifdef PS2_KBD_ENABLE
    ps2_set_event_callback(kbd_on_event);
    ps2_pio_init();
#endif

#ifdef USB_KBD_ENABLE
    kbd_usb_init();
    kbd_usb_set_event_callback(kbd_on_event);
#endif
}

#endif // KBD_ENABLE
