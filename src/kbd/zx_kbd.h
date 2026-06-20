/**
 * zx_kbd.h - Universal to ZX Spectrum keyboard mapping
 *
 * ZX Spectrum keyboard: 8 rows × 5 columns matrix.
 */

#pragma once

#include "key_codes.h"

typedef struct
{
    union
    {
        uint32_t u[2];
        uint8_t a[8];
    };
} zx_kbd_state_t;

void __not_in_flash_func(zx_kbd_set_state)(zx_kbd_state_t *zx_keys_matrix, kbd_state_t *kb_state);
