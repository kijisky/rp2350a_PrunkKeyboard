"""
boot.py — runs once at board start, before code.py.

Task: configure the USB HID descriptor of the device that the host PC
sees (the Pico) — keyboard only, with no extra interfaces (the CDC
console can be kept for serial debugging, but HID must be exactly one
boot keyboard so the OS recognizes the device as a regular keyboard
right away).

boot.py has no access to the USB host API (usb.core) — that is only
available in code.py, once the main USB stack is up.

Mode: work mode is the default and exposes only the keyboard HID
device. Debug mode, selected by the Ctrl+Alt+ё combo in code.py, skips
the custom HID setup and leaves CircuitPython's default USB devices.
The mode is stored in microcontroller.nvm, so it survives reboots.
"""

import microcontroller
import usb_hid

# Standard HID report descriptor for a keyboard (boot keyboard,
# 8-byte report: modifier, reserved, 6 keycodes) — this is exactly
# what code.py expects when it calls usb_hid.devices and
# kbd_out.send_report().
KEYBOARD_REPORT_DESCRIPTOR = bytes((
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x06,        # Usage (Keyboard)
    0xA1, 0x01,        # Collection (Application)
    0x05, 0x07,        #   Usage Page (Key Codes)
    0x19, 0xE0,        #   Usage Minimum (224)
    0x29, 0xE7,        #   Usage Maximum (231)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x08,        #   Report Count (8)
    0x81, 0x02,        #   Input (Data, Variable, Absolute) — modifier byte
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x08,        #   Report Size (8)
    0x81, 0x01,        #   Input (Constant) — reserved byte
    0x95, 0x05,        #   Report Count (5)
    0x75, 0x01,        #   Report Size (1)
    0x05, 0x08,        #   Usage Page (LEDs)
    0x19, 0x01,        #   Usage Minimum (1)
    0x29, 0x05,        #   Usage Maximum (5)
    0x91, 0x02,        #   Output (Data, Variable, Absolute) — LED report
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x03,        #   Report Size (3)
    0x91, 0x01,        #   Output (Constant)
    0x95, 0x06,        #   Report Count (6)
    0x75, 0x08,        #   Report Size (8)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x65,        #   Logical Maximum (101)
    0x05, 0x07,        #   Usage Page (Key Codes)
    0x19, 0x00,        #   Usage Minimum (0)
    0x29, 0x65,        #   Usage Maximum (101)
    0x81, 0x00,        #   Input (Data, Array) — 6 keycodes
    0xC0,              # End Collection
))

keyboard = usb_hid.Device(
    report_descriptor=KEYBOARD_REPORT_DESCRIPTOR,
    usage_page=0x01,
    usage=0x06,
    report_ids=(0,),
    in_report_lengths=(8,),
    out_report_lengths=(0,),
)

# nvm[0] == 1 means debug mode (see the module docstring).
try:
    DEBUG_MODE = microcontroller.nvm[0] == 1
except Exception:
    DEBUG_MODE = False

# Work mode: expose exactly one HID device to the computer — the keyboard.
if not DEBUG_MODE:
    usb_hid.enable((keyboard,))
