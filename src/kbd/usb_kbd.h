/**
 * usb_kbd.h - USB HID keyboard and mouse driver using TinyUSB Host
 */

#pragma once

#include "key_codes.h"

typedef void (*usb_kbd_event_fn)(void);

typedef struct
{
    uint8_t buttons; // bits 0-2: buttons (active low), bit 3: always 1,
                     // bits 4-7: wheel position (4-bit) or 0xF if no wheel
    uint8_t x;       // accumulated X position (wraps 0-255)
    uint8_t y;       // accumulated Y position (wraps 0-255)
    // bool has_wheel;  // true if mouse reported wheel data
    // uint8_t wheel;   // accumulated wheel position (4-bit, wraps 0-15)
} usb_mouse_state_t;

void usb_kbd_init(void);
void usb_kbd_task(void);
void usb_kbd_set_event_callback(usb_kbd_event_fn cb);
kbd_state_t *usb_kbd_get_state(void);
usb_mouse_state_t *usb_mouse_get_state(void);
