/**
 * kbd_ch446q.c - CH446Q analog switch driver
 */
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "g_config.h"
#include "kbd_ch446q.h"

#define BUSY_WAIT_US(x)      \
    for (int jj = 10; jj--;) \
    asm volatile("nop \n")

void __not_in_flash_func(ch446q_set_switch)(int Y, int X, bool state)
{
    gpio_put(CH446Q_PIN_CLK, 0);
    gpio_put(CH446Q_PIN_STB, 0);

    for (int i = 3; i--;)
    {
        gpio_put(CH446Q_PIN_DATA, Y & 0x4);
        BUSY_WAIT_US(1);
        gpio_put(CH446Q_PIN_CLK, 1);
        BUSY_WAIT_US(1);
        gpio_put(CH446Q_PIN_CLK, 0);
        Y <<= 1;
    }

    for (int i = 4; i--;)
    {
        gpio_put(CH446Q_PIN_DATA, X & 0x8);
        BUSY_WAIT_US(1);
        gpio_put(CH446Q_PIN_CLK, 1);
        BUSY_WAIT_US(1);
        gpio_put(CH446Q_PIN_CLK, 0);
        X <<= 1;
    }

    gpio_put(CH446Q_PIN_DATA, state);
    BUSY_WAIT_US(1);
    gpio_put(CH446Q_PIN_STB, 1);
    BUSY_WAIT_US(1);
    gpio_put(CH446Q_PIN_STB, 0);
}

void ch446q_reset(void)
{
    for (int i = 0; i < 128; i++)
    {
        ch446q_set_switch(i >> 4, i & 0xF, false);
    }
}

void ch446q_init(void)
{
    gpio_init(CH446Q_PIN_DATA);
    gpio_set_dir(CH446Q_PIN_DATA, GPIO_OUT);

    gpio_init(CH446Q_PIN_CLK);
    gpio_set_dir(CH446Q_PIN_CLK, GPIO_OUT);

    gpio_init(CH446Q_PIN_STB);
    gpio_set_dir(CH446Q_PIN_STB, GPIO_OUT);

    ch446q_reset();
}
