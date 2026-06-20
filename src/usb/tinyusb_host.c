/**
 * tinyusb_host.c - TinyUSB Host sources for USB Host HID mode
 *
 * The Arduino framework's libpico.a only includes TinyUSB device-mode code.
 * This file compiles the TinyUSB host stack by including sources directly.
 * Each source must be in its own translation unit to avoid redefinition errors,
 * so we only include the host core here (usbh.c + hub.c).
 * Other sources are in separate files.
 *
 * Only compiled when USB_KBD_ENABLE is defined.
 */

#include "g_config.h"

#ifdef USB_KBD_ENABLE

// TinyUSB host core
#include "host/usbh.c"
#include "host/hub.c"

#endif // USB_KBD_ENABLE
