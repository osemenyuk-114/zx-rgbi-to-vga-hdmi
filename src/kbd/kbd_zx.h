/**
 * kbd_zx.h - Universal to ZX Spectrum keyboard mapping
 *
 * ZX Spectrum keyboard: 8 rows × 5 columns matrix.
 */

#pragma once

#include <stdint.h>
#include "kbd_codes.h"

typedef struct {
    union {
        uint32_t u[2];
        uint8_t  a[8];
    };
} zx_kb_state_t;

void __not_in_flash_func(set_zx_kb_state)(zx_kb_state_t* zx_keys_matrix, kb_state_t* kb_state);
