/**
 * kbd_ch446q.h - CH446Q analog switch driver
 *
 * 3-wire serial interface: DATA, CLK, STB.
 * Pins defined per-board in g_config.h.
 */

#pragma once

#include <stdbool.h>
#include "kbd_zx.h"

void ch446q_init(void);
void ch446q_reset(void);
void __not_in_flash_func(ch446q_set_switch)(int Y, int X, bool state);
