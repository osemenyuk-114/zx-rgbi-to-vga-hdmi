/**
 * tinyusb_host_hcd.c - TinyUSB RP2040 Host Controller Driver
 * Separate TU to avoid redefinition conflicts.
 */
#include "g_config.h"
#ifdef USB_KBD_ENABLE
#include "portable/raspberrypi/rp2040/hcd_rp2040.c"
#endif
