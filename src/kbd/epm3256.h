/**
 * epm3256.h - PIO-based SPI keyboard+mouse output driver (EPM3256 CPLD)
 *
 * Shifts out ZX keyboard matrix (8×5) and Kempston mouse state
 * as a serial bitstream via PIO. Used on boards with shift-register
 * keyboard interface instead of CH446Q.
 *
 * Pins: KBD_PIN_DATA (data), KBD_PIN_CLK (clock), KBD_PIN_STB (latch)
 * defined per-board in g_config.h.
 */

#pragma once

#include "zx_kbd.h"

void epm3256_init(void);
void __not_in_flash_func(epm3256_send)(uint8_t special_keys,
                                       uint8_t mouse_keys,
                                       uint8_t mouse_x,
                                       uint8_t mouse_y,
                                       zx_kbd_state_t *zx_keys_matrix);
