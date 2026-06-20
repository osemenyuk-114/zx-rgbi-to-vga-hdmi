/**
 * kbd.h - Universal keyboard dispatcher
 *
 * Polls input backends (PS/2, USB), merges state, tracks edges,
 * and routes to ZX Spectrum output backend (CH446Q).
 */

#pragma once

#include <stdint.h>

void kbd_init(void);

// Toggles on each keyboard/mouse event (odd = active)
extern volatile uint32_t kbd_activity_cnt;
