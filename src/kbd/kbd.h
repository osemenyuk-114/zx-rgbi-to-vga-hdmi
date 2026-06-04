/**
 * kbd.h - Universal keyboard dispatcher
 *
 * Polls input backends (PS/2, USB), merges state, tracks edges,
 * and routes to ZX Spectrum output backend (CH446Q).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "g_config.h"

#ifdef KBD_ENABLE

#include "kbd_codes.h"

void kbd_init(void);

#endif // KBD_ENABLE
