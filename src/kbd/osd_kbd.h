/**
 * osd_kbd.h - Keyboard-to-OSD bridge
 *
 * Translates keyboard input into OSD virtual button presses and
 * menu activation requests.  Runs across two cores:
 *   Core 1: osd_kbd_intercept() — called from PS/2 / USB IRQ
 *   Core 0: osd_kbd_apply_virtual() — called from osd_buttons_update()
 */

#pragma once

#include "g_config.h"

#if defined(KBD_ENABLE) && defined(OSD_ENABLE)

#include "key_codes.h"

// Virtual button bits (set on Core 1, consumed on Core 0)
#define OSD_VIRT_UP 1
#define OSD_VIRT_DOWN 2
#define OSD_VIRT_SEL 4
#define OSD_VIRT_BACK 8

// Cross-core volatile flags
extern volatile uint8_t osd_virtual_buttons; // Edge-triggered events (SEL, BACK)
extern volatile uint8_t osd_virtual_held;    // Level state: which arrows/keys are currently held
extern volatile bool osd_menu_request;       // F11 → toggle OSD menu
extern volatile bool ff_osd_request;         // F12 → toggle Gotek OSD

// True while keyboard is driving OSD (suppresses ZX output in kbd.c)
extern bool osd_kbd_active;

/**
 * Core 1 entry point — called from kbd_on_event() after state update.
 * Detects hotkeys (F11, F12) and maps arrow/Enter/ESC to
 * virtual buttons when the OSD menu is active.
 */
void __not_in_flash_func(osd_kbd_intercept)(kbd_state_t *state);

/**
 * Core 0 — inject virtual button presses into osd_buttons.
 * Implements controlled repeat for arrow keys (independent of PS/2 typematic)
 * and updates key_held[]/key_hold_start[] for parameter acceleration.
 * Call at the END of osd_buttons_update(), after the GPIO loop.
 */
void osd_kbd_apply_virtual(void);

#endif // KBD_ENABLE && OSD_ENABLE
