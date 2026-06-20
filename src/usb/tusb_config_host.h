/**
 * tusb_config_host.h - TinyUSB configuration (dual-mode)
 *
 * Replaces the default Arduino tusb_config.h.
 * - When USB_KBD_ENABLE is defined: USB Host mode for HID keyboard
 * - Otherwise: USB Device mode (standard Arduino CDC Serial)
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    //--------------------------------------------------------------------
    // COMMON CONFIGURATION
    //--------------------------------------------------------------------

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifdef USB_KBD_ENABLE
// USB Host mode on rhport0
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST
#else
// USB Device mode on rhport0 (standard Arduino)
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#endif

#define CFG_TUSB_OS OPT_OS_PICO

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

    //--------------------------------------------------------------------
    // Mode-specific configuration
    //--------------------------------------------------------------------

#ifdef USB_KBD_ENABLE

    //--------------------------------------------------------------------
    // --- HOST MODE ---
    //--------------------------------------------------------------------

#define CFG_TUH_DEVICE_MAX 5 // Hub + 4 devices (arbitrary, can be increased if needed)

#ifndef CFG_TUH_ENDPOINT_MAX
#define CFG_TUH_ENDPOINT_MAX 8
#endif

#ifndef CFG_TUH_ENUMERATION_BUFSIZE
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#endif

// Host classes
#define CFG_TUH_HUB 1
#define CFG_TUH_HID 2
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64
#define CFG_TUH_CDC 0
#define CFG_TUH_MSC 0
#define CFG_TUH_VENDOR 0

// No device classes in host mode
#define CFG_TUD_HID 0
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0
#define CFG_TUD_NCM 0

#else // !USB_KBD_ENABLE

//--------------------------------------------------------------------
// --- DEVICE MODE (Arduino default) ---
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

#define CFG_TUD_HID (2)
#define CFG_TUD_CDC (1)
#define CFG_TUD_MSC (1)
#define CFG_TUD_MIDI (1)
#define CFG_TUD_VENDOR (0)
#define CFG_TUD_NCM (1)

#define CFG_TUD_CDC_RX_BUFSIZE (256)
#define CFG_TUD_CDC_TX_BUFSIZE (256)
#define CFG_TUD_MSC_EP_BUFSIZE (64)
#define CFG_TUD_HID_EP_BUFSIZE (64)
#define CFG_TUD_MIDI_RX_BUFSIZE (64)
#define CFG_TUD_MIDI_TX_BUFSIZE (64)

#include "lwipopts.h"
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE (2 * TCP_MSS + 100)
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE (2 * TCP_MSS + 100)

#ifndef CFG_TUD_NCM_OUT_NTB_N
#define CFG_TUD_NCM_OUT_NTB_N 1
#endif

#ifndef CFG_TUD_NCM_IN_NTB_N
#define CFG_TUD_NCM_IN_NTB_N 1
#endif

#endif // USB_KBD_ENABLE

#ifdef __cplusplus
}
#endif
