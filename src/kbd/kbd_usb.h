/**
 * kbd_usb.h - USB HID keyboard driver using TinyUSB Host
 */

#pragma once

#include "kbd_codes.h"

typedef void (*usb_event_fn)(void);

void kbd_usb_init(void);
void kbd_usb_task(void);
void kbd_usb_set_event_callback(usb_event_fn cb);
kb_state_t *kbd_usb_get_kb_state(void);
