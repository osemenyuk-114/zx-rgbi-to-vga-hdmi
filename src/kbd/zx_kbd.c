/**
 * zx_kbd.c - Universal to ZX Spectrum keyboard mapping
 *
 * ZX Spectrum Keyboard Matrix (row, bit):
 * Row 0: CAPS(0), Z(1), X(2), C(3), V(4)
 * Row 1: A(0), S(1), D(2), F(3), G(4)
 * Row 2: Q(0), W(1), E(2), R(3), T(4)
 * Row 3: 1(0), 2(1), 3(2), 4(3), 5(4)
 * Row 4: 0(0), 9(1), 8(2), 7(3), 6(4)
 * Row 5: P(0), O(1), I(2), U(3), Y(4)
 * Row 6: ENTER(0), L(1), K(2), J(3), H(4)
 * Row 7: SPACE(0), SYM_SHIFT(1), M(2), N(3), B(4)
 */

#include "g_config.h"
#include "zx_kbd.h"

// Pack row (0-7) and column bitmask into one byte: bits 7..5 = row, bits 4..0 = bitmask

#define ZX(row, col) (((row) << 5) | (1 << (col)))
#define ZX_NONE 0

#define ZX_CS ZX(0, 0) // CAPS SHIFT
#define ZX_SS ZX(7, 1) // SYMBOL SHIFT

// Mapping table: universal key code → ZX matrix position(s)

typedef struct
{
    uint8_t key_code;
    uint8_t zx1; // First ZX key (or modifier)
    uint8_t zx2; // Second ZX key (ZX_NONE if single)
} zx_key_map_t;

static const zx_key_map_t zx_key_map[] = {
    // Letters
    {KEY_A, ZX(1, 0), ZX_NONE},
    {KEY_B, ZX(7, 4), ZX_NONE},
    {KEY_C, ZX(0, 3), ZX_NONE},
    {KEY_D, ZX(1, 2), ZX_NONE},
    {KEY_E, ZX(2, 2), ZX_NONE},
    {KEY_F, ZX(1, 3), ZX_NONE},
    {KEY_G, ZX(1, 4), ZX_NONE},
    {KEY_H, ZX(6, 4), ZX_NONE},
    {KEY_I, ZX(5, 2), ZX_NONE},
    {KEY_J, ZX(6, 3), ZX_NONE},
    {KEY_K, ZX(6, 2), ZX_NONE},
    {KEY_L, ZX(6, 1), ZX_NONE},
    {KEY_M, ZX(7, 2), ZX_NONE},
    {KEY_N, ZX(7, 3), ZX_NONE},
    {KEY_O, ZX(5, 1), ZX_NONE},
    {KEY_P, ZX(5, 0), ZX_NONE},
    {KEY_Q, ZX(2, 0), ZX_NONE},
    {KEY_R, ZX(2, 3), ZX_NONE},
    {KEY_S, ZX(1, 1), ZX_NONE},
    {KEY_T, ZX(2, 4), ZX_NONE},
    {KEY_U, ZX(5, 3), ZX_NONE},
    {KEY_V, ZX(0, 4), ZX_NONE},
    {KEY_W, ZX(2, 1), ZX_NONE},
    {KEY_X, ZX(0, 2), ZX_NONE},
    {KEY_Y, ZX(5, 4), ZX_NONE},
    {KEY_Z, ZX(0, 1), ZX_NONE},

    // Numbers
    {KEY_0, ZX(4, 0), ZX_NONE},
    {KEY_1, ZX(3, 0), ZX_NONE},
    {KEY_2, ZX(3, 1), ZX_NONE},
    {KEY_3, ZX(3, 2), ZX_NONE},
    {KEY_4, ZX(3, 3), ZX_NONE},
    {KEY_5, ZX(3, 4), ZX_NONE},
    {KEY_6, ZX(4, 4), ZX_NONE},
    {KEY_7, ZX(4, 3), ZX_NONE},
    {KEY_8, ZX(4, 2), ZX_NONE},
    {KEY_9, ZX(4, 1), ZX_NONE},

    // Direct keys
    {KEY_ENTER, ZX(6, 0), ZX_NONE},
    {KEY_SPACE, ZX(7, 0), ZX_NONE},

    // Modifiers
    {KEY_L_SHIFT, ZX_CS, ZX_NONE}, // CAPS SHIFT
    {KEY_R_SHIFT, ZX_CS, ZX_NONE}, // CAPS SHIFT
    {KEY_L_CTRL, ZX_SS, ZX_NONE},  // SYMBOL SHIFT
    {KEY_R_CTRL, ZX_SS, ZX_NONE},  // SYMBOL SHIFT

    // CAPS SHIFT + key
    {KEY_BACK_SPACE, ZX_CS, ZX(4, 0)}, // CS+0 = DELETE
    {KEY_DELETE, ZX_CS, ZX(4, 0)},     // CS+0 = DELETE
    {KEY_CAPS_LOCK, ZX_CS, ZX(3, 1)},  // CS+2 = CAPS LOCK
    {KEY_ESC, ZX_CS, ZX(7, 0)},        // CS+SPACE = BREAK
    {KEY_TAB, ZX_CS, ZX(3, 0)},        // CS+1 = EDIT
    {KEY_PAGE_UP, ZX_CS, ZX(3, 2)},    // CS+3 = TRUE VIDEO
    {KEY_PAGE_DOWN, ZX_CS, ZX(3, 3)},  // CS+4 = INVERSE VIDEO

    // Arrow keys: CAPS SHIFT + 5/6/7/8
    {KEY_UP, ZX_CS, ZX(4, 3)},    // CS+7
    {KEY_DOWN, ZX_CS, ZX(4, 4)},  // CS+6
    {KEY_LEFT, ZX_CS, ZX(3, 4)},  // CS+5
    {KEY_RIGHT, ZX_CS, ZX(4, 2)}, // CS+8

    // SYMBOL SHIFT + key (punctuation)
    {KEY_QUOTE, ZX_SS, ZX(5, 0)},     // SS+P = "
    {KEY_COMMA, ZX_SS, ZX(7, 3)},     // SS+N = ,
    {KEY_PERIOD, ZX_SS, ZX(7, 2)},    // SS+M = .
    {KEY_SLASH, ZX_SS, ZX(0, 4)},     // SS+V = /
    {KEY_SEMICOLON, ZX_SS, ZX(5, 1)}, // SS+O = ;
    {KEY_MINUS, ZX_SS, ZX(6, 3)},     // SS+J = -
    {KEY_EQUALS, ZX_SS, ZX(6, 1)},    // SS+L = =

    // Numeric keypad → numbers
    {KEY_NUM_0, ZX(4, 0), ZX_NONE},
    {KEY_NUM_1, ZX(3, 0), ZX_NONE},
    {KEY_NUM_2, ZX(3, 1), ZX_NONE},
    {KEY_NUM_3, ZX(3, 2), ZX_NONE},
    {KEY_NUM_4, ZX(3, 3), ZX_NONE},
    {KEY_NUM_5, ZX(3, 4), ZX_NONE},
    {KEY_NUM_6, ZX(4, 4), ZX_NONE},
    {KEY_NUM_7, ZX(4, 3), ZX_NONE},
    {KEY_NUM_8, ZX(4, 2), ZX_NONE},
    {KEY_NUM_9, ZX(4, 1), ZX_NONE},
    {KEY_NUM_ENTER, ZX(6, 0), ZX_NONE}, // ENTER

    // Numeric keypad → symbols
    {KEY_NUM_SLASH, ZX_SS, ZX(0, 4)},  // SS+V = /
    {KEY_NUM_MULT, ZX_SS, ZX(7, 4)},   // SS+B = *
    {KEY_NUM_MINUS, ZX_SS, ZX(6, 3)},  // SS+J = -
    {KEY_NUM_PLUS, ZX_SS, ZX(6, 2)},   // SS+K = +
    {KEY_NUM_PERIOD, ZX_SS, ZX(7, 2)}, // SS+M = .
};

#define ZX_KEY_MAP_SIZE (count_of(zx_key_map))

// Fast lookup: key_code → index into zx_key_map (+1), 0 = unmapped

#define ZX_LUT_SIZE 106

static uint8_t zx_key_lut[ZX_LUT_SIZE];
static bool zx_lut_ready = false;

static void zx_build_lut(void)
{
    for (int i = 0; i < ZX_LUT_SIZE; i++)
        zx_key_lut[i] = 0;

    for (int i = 0; i < (int)ZX_KEY_MAP_SIZE; i++)
    {
        uint8_t kc = zx_key_map[i].key_code;

        if (kc < ZX_LUT_SIZE)
            zx_key_lut[kc] = i + 1; // +1 so 0 means "not mapped"
    }

    zx_lut_ready = true;
}

static inline void apply_zx_entry(zx_kbd_state_t *zx, const zx_key_map_t *e)
{
    // Decode packed position: bits 7..5 = row, bits 4..0 = bitmask
    zx->a[e->zx1 >> 5] |= (e->zx1 & 0x1F);

    if (e->zx2 != ZX_NONE)
        zx->a[e->zx2 >> 5] |= (e->zx2 & 0x1F);
}

void __not_in_flash_func(zx_kbd_set_state)(zx_kbd_state_t *zx_keys_matrix, kbd_state_t *kb_state)
{
    if (!zx_lut_ready)
        zx_build_lut();

    // Clear output matrix
    zx_keys_matrix->u[0] = 0;
    zx_keys_matrix->u[1] = 0;

    // Scan all pressed keys via bitmask
    const uint32_t *words = (const uint32_t *)kb_state;

    for (int w = 0; w < 4; w++)
    {
        uint32_t mask = words[w];

        if (!mask)
            continue;

        int base = w << 5; // w * 32

        while (mask)
        {
            // Find lowest set bit
            int bit = __builtin_ctz(mask);
            int key_code = base + bit;

            // Look up in LUT
            if (key_code < ZX_LUT_SIZE)
            {
                uint8_t idx = zx_key_lut[key_code];

                if (idx)
                    apply_zx_entry(zx_keys_matrix, &zx_key_map[idx - 1]);
            }

            // Clear lowest set bit
            mask &= mask - 1;
        }
    }
}
