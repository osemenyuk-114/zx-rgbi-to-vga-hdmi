#include "g_config.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "led.h"

static uint8_t led_r, led_g, led_b;

#ifdef WS2812_LED_ENABLE
#include "ws2812.pio.h"

#define WS2812_PIN 16
// clkdiv = sys_clk / (800kHz * 10 cycles/bit).  270MHz → 33.75, 252MHz → 31.5, 125MHz → 15.6
// Fixed 34.0 works for all modes: bit rate 794–368 kHz (WS2812 tolerates 400–1000 kHz).
#define WS2812_PIO_CLKDIV 34.0f

void led_init()
{
    uint offset = 0;
    pio_add_program_at_offset(PIO_WS2812, &pio_ws2812_program, offset);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + pio_ws2812_program.length - 1);
    sm_config_set_out_shift(&c, false, false, 32); // MSB first, no autopull
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_gpio_init(PIO_WS2812, WS2812_PIN);
    pio_sm_set_consecutive_pindirs(PIO_WS2812, SM_WS2812, WS2812_PIN, 1, true);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, WS2812_PIN);
    sm_config_set_clkdiv(&c, WS2812_PIO_CLKDIV);

    pio_sm_init(PIO_WS2812, SM_WS2812, offset, &c);
    pio_sm_set_enabled(PIO_WS2812, SM_WS2812, true);

    led_r = led_g = led_b = 0;
}

void led_put(uint8_t color, uint8_t brightness)
{
    switch (color)
    {
    case LED_R:
        if (led_r == brightness)
            return;

        led_r = brightness;
        break;

    case LED_G:
        if (led_g == brightness)
            return;

        led_g = brightness;
        break;

    case LED_B:
        if (led_b == brightness)
            return;

        led_b = brightness;
        break;
    }

    uint32_t rgb = ((uint32_t)led_r << 16) | ((uint32_t)led_g << 8) | (uint32_t)led_b;
    pio_sm_put_blocking(PIO_WS2812, SM_WS2812, rgb << 8);
}

#elif defined(HW_CONFIG_ENABLE) // No free GPIO pins for LED
void led_init()
{
}

void led_put(uint8_t color, uint8_t brightness)
{
}

#else // If WS2812 is not enabled and HW_CONFIG is not enabled, use PWM for LED
#include "hardware/pwm.h"

#define LED_PIN 25

void led_init()
{
    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(LED_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 254);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(LED_PIN, 0);

    led_r = led_g = led_b = 0;
}

void led_put(uint8_t color, uint8_t brightness)
{
    switch (color)
    {
    case LED_R:
        led_r = brightness;
        break;

    case LED_G:
        led_g = brightness;
        break;

    case LED_B:
        led_b = brightness;
        break;
    }

    uint8_t val = led_r;
    if (led_g > val)
        val = led_g;

    if (led_b > val)
        val = led_b;

    pwm_set_gpio_level(LED_PIN, val);
}

#endif
