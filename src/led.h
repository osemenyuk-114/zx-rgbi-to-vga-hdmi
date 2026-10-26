#pragma once

#include <stdint.h>
#include <stdbool.h>

// Virtual LED color channels
#define LED_R 0
#define LED_G 1
#define LED_B 2

void led_init();
void led_put(uint8_t color, uint8_t brightness);