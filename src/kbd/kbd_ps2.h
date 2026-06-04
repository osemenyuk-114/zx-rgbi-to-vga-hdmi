/**
 * kbd_ps2.h - PIO-based PS/2 keyboard driver
 *
 * IRQ-driven: calls registered callback on each key event.
 * Pins defined per-board in g_config.h (PS2_PIN_DATA, PS2_PIN_CLK).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"
#include "kbd_codes.h"

typedef void (*ps2_event_fn)(void);

void ps2_pio_init(void);
void ps2_set_event_callback(ps2_event_fn cb);
kb_state_t* ps2_get_kb_state(void);
void ps2_clear_kb_state(void);
