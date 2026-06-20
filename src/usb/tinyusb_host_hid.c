/**
 * tinyusb_host_hid.c - TinyUSB HID host class driver
 * Separate TU to avoid redefinition conflicts.
 */
#include "g_config.h"
#ifdef USB_KBD_ENABLE
#include "class/hid/hid_host.c"
#endif
