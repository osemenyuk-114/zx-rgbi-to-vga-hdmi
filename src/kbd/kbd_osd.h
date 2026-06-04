/**
 * kbd_osd.h - Keyboard-to-OSD bridge
 *
 * Translates keyboard input into OSD virtual button presses and
 * menu activation requests.  Runs across two cores:
 *   Core 1: kbd_osd_intercept() — called from PS/2 / USB IRQ
 *   Core 0: kbd_osd_apply_virtual() — called from osd_buttons_update()
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "g_config.h"

#if defined(KBD_ENABLE) && defined(OSD_ENABLE)

#include "kbd_codes.h"

// Virtual button bits (set on Core 1, consumed on Core 0)
#define OSD_VIRT_UP   1
#define OSD_VIRT_DOWN 2
#define OSD_VIRT_SEL  4
#define OSD_VIRT_BACK 8

// Cross-core volatile flags
extern volatile uint8_t osd_virtual_buttons;  // Edge-triggered events (SEL, BACK)
extern volatile uint8_t osd_virtual_held;     // Level state: which arrows/keys are currently held
extern volatile bool osd_menu_request;    // PrtScr: request to open/toggle OSD menu
extern volatile bool osd_gotek_request;   // ScrLk:  request Gotek FF OSD SEL press

// True while keyboard is driving OSD (suppresses ZX output in kbd.c)
extern bool kbd_osd_active;

/**
 * Core 1 entry point — called from kbd_on_event() after state update.
 * Detects hotkeys (PrtScr, ScrLk) and maps arrow/Enter/ESC to
 * virtual buttons when the OSD menu is active.
 */
void __not_in_flash_func(kbd_osd_intercept)(kb_state_t *state);

/**
 * Core 0 — inject virtual button presses into osd_buttons.
 * Implements controlled repeat for arrow keys (independent of PS/2 typematic)
 * and updates key_held[]/key_hold_start[] for parameter acceleration.
 * Call at the END of osd_buttons_update(), after the GPIO loop.
 */
void kbd_osd_apply_virtual(void);

#endif // KBD_ENABLE && OSD_ENABLE
