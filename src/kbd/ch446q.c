/**
 * ch446q.c - CH446Q analog switch driver (PIO-based)
 *
 * Shifts out 8-bit commands via PIO: Y[2:0] X[3:0] state
 * 7 address bits clocked with CLK rising edge, state latched with STB pulse.
 *
 * Pin layout (must be consecutive):
 *   OUT pin:    KBD_PIN_DATA
 *   Sideset[0]: KBD_PIN_CLK
 *   Sideset[1]: KBD_PIN_STB  (= CLK + 1)
 */

#include "hardware/clocks.h"

#include "g_config.h"
#include "ch446q.h"
#include "kbd.pio.h"

#ifndef SPI_KB_ENABLE

void __not_in_flash_func(ch446q_set_switch)(int Y, int X, bool state)
{
    // Pack Y[2:0] X[3:0] state into bits [31:24] (MSB first shift)
    uint32_t data = ((uint32_t)(Y & 0x7) << 5) |
                    ((uint32_t)(X & 0xF) << 1) |
                    (state ? 1u : 0u);
    data <<= 24;

    while (PIO_CH446Q->fstat & (1u << (PIO_FSTAT_TXFULL_LSB + SM_CH446Q)))
        ;

    PIO_CH446Q->txf[SM_CH446Q] = data;
}

void ch446q_reset(void)
{
    for (int i = 0; i < 128; i++)
        ch446q_set_switch(i >> 4, i & 0xF, false);
}

void ch446q_init(void)
{
    // Load at fixed offset right after PS2 program (static — never unloaded)
    uint offset = pio_ps2_wrap + 1;
    pio_add_program_at_offset(PIO_CH446Q, &pio_ch446q_program, offset);
    pio_sm_config c = pio_get_default_sm_config();

    sm_config_set_wrap(&c, offset, offset + pio_ch446q_program.length - 1);
    sm_config_set_out_shift(&c, false, false, 32); // MSB first, no autopull
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Data pin (OUT)
    pio_gpio_init(PIO_CH446Q, KBD_PIN_DATA);
    pio_sm_set_consecutive_pindirs(PIO_CH446Q, SM_CH446Q, KBD_PIN_DATA, 1, true);
    sm_config_set_out_pin_base(&c, KBD_PIN_DATA);
    sm_config_set_out_pin_count(&c, 1);

    // Sideset pins: CLK (bit 0), STB (bit 1) — consecutive
    pio_gpio_init(PIO_CH446Q, KBD_PIN_CLK);
    pio_gpio_init(PIO_CH446Q, KBD_PIN_STB);
    pio_sm_set_pindirs_with_mask(PIO_CH446Q, SM_CH446Q,
                                 (3u << KBD_PIN_CLK) | (1u << KBD_PIN_DATA),
                                 (3u << KBD_PIN_CLK) | (1u << KBD_PIN_DATA));
    sm_config_set_sideset_pins(&c, KBD_PIN_CLK);
    sm_config_set_sideset(&c, 2, false, false);

    // Target ~2.5 MHz PIO clock (~0.4µs per cycle), safe for CH446Q (max ~20 MHz)
    float fdiv = (float)clock_get_hz(clk_sys) / 2500000.0f;
    sm_config_set_clkdiv(&c, fdiv);

    pio_sm_init(PIO_CH446Q, SM_CH446Q, offset, &c);
    pio_sm_set_enabled(PIO_CH446Q, SM_CH446Q, true);

    ch446q_reset();
}

#endif // !SPI_KB_ENABLE
