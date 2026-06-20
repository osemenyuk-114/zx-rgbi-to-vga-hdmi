/**
 * ps2_kbd.h - PIO-based PS/2 keyboard driver
 *
 * IRQ-driven: calls registered callback on each key event.
 * Pins defined per-board in g_config.h (PS2_PIN_DATA, PS2_PIN_CLK).
 */

#pragma once

#include "key_codes.h"

typedef void (*ps2_kbd_event_fn)(void);

void ps2_kbd_pio_init(void);
void ps2_kbd_set_event_callback(ps2_kbd_event_fn cb);
kbd_state_t *ps2_kbd_get_state(void);
void ps2_kbd_clear_state(void);
