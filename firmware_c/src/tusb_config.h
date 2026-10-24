// tusb_config.h — TinyUSB dual-role configuration: native USB
// (RHPORT0) as a HID+CDC device to the PC, PIO-USB (RHPORT1) as a
// host for the physical keyboard. Based on TinyUSB's own
// examples/dual/host_hid_to_device_cdc config, trimmed to what this
// project actually uses (keyboard only, no mouse/generic HID).
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

// RHPORT0 = native RP2350 USB controller -> device, talks to the PC.
// RHPORT1 = PIO-USB (bit-banged on two GPIOs, see README) -> host,
// talks to the physical keyboard.
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

//--------------------------------------------------------------------
// DEVICE CONFIGURATION (RHPORT0)
//--------------------------------------------------------------------

#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_HID    1
#define CFG_TUD_CDC    1
#define CFG_TUD_MSC    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_HID_EP_BUFSIZE 8

#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256

//--------------------------------------------------------------------
// HOST CONFIGURATION (RHPORT1, via PIO-USB)
//--------------------------------------------------------------------

// Tells TinyUSB's rp2040 board support to back RHPORT1 with
// Pico-PIO-USB instead of a second native controller (RP2350A has
// only one native USB controller).
#define CFG_TUH_RPI_PIO_USB 1

#define CFG_TUH_ENDPOINT_MAX 16

#define CFG_TUH_HUB    1  // let the keyboard sit behind a hub, if ever
#define CFG_TUH_HID    4  // a few HID interfaces (keyboard + boot mouse, etc.)
#define CFG_TUH_CDC    0
#define CFG_TUH_MSC    0
#define CFG_TUH_VENDOR 0

#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif
