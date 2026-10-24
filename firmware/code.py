"""
code.py — the main device loop.

Architecture: the RP2350A board is connected to two things at once:
  - the computer, via the board's native USB port, as a USB HID
    keyboard (`usb_hid`, configured in boot.py) — this is what the
    computer sees;
  - the physical keyboard, via the board's second USB-A port, which
    works in USB HOST mode (`usb.core`, native RP2350 support in
    CircuitPython 9.x on boards with two USB ports / USB-OTG) — the
    device reads raw boot-keyboard reports from it.

In other words: the physical keyboard is plugged INTO the board, and
the board is plugged INTO the computer. Everything the user types goes
through this loop, which either passes the report through unchanged or
replaces it with a macro from config.MACROS.

This is a MITM ("man in the middle") at the HID protocol level: the
device fully controls what the computer sees of what was typed. See
the "Ethics and limits of use" section of README.md before using this
on a device that you do not personally own.
"""

import time
import usb.core
import usb_hid

import config
from keymap import BACKSPACE, MOD_OTHER, SCAN_BASE, key_of, norm_trigger

kbd_out = next(d for d in usb_hid.devices if d.usage_page == 0x01 and d.usage == 0x06)

EP_IN = 0x81
buf = bytearray(8)

# Compile the config once at startup — a typo in config.py will fail
# right away at load time, not in the middle of the device's work.
MACROS_C = [
    (norm_trigger(trig), [key_of(tok) for tok in seq])
    for trig, seq in config.MACROS.items()
]
MAXLEN = max(len(trig) for trig, _ in MACROS_C) if MACROS_C else 0
FILTER_C = {key_of(tok)[1] for tok in config.FILTER}


def play(seq):
    """Play the macro's key sequence to the computer."""
    kbd_out.send_report(bytes(8))  # release everything, incl. a held Shift
    for mod, kc in seq:
        kbd_out.send_report(bytes([mod, 0, kc, 0, 0, 0, 0, 0]))
        kbd_out.send_report(bytes(8))


def build(src, muted):
    """Copy of the input report with filtered/muted keycodes zeroed out
    (the computer won't see them)."""
    out = bytearray(src)
    for i in range(2, 8):
        if out[i] in FILTER_C or out[i] in muted:
            out[i] = 0
    return out


def connect():
    """Wait for the physical keyboard to be connected to the host port,
    and take control from CircuitPython's built-in HID driver (which
    otherwise tries to configure it on its own)."""
    while True:
        for dev in usb.core.find(find_all=True):
            try:
                if dev.is_kernel_driver_active(0):
                    dev.detach_kernel_driver(0)
                dev.set_configuration()
                try:
                    dev.ctrl_transfer(0x21, 0x0B, 0, 0)  # SET_PROTOCOL = Boot
                except Exception:
                    pass
                print("Keyboard connected")
                return dev
            except Exception as e:
                print("Connect error:", e)
        time.sleep(0.5)


# === Main loop ====================================================
dev = connect()
prev = []        # keycodes held down in the previous report
typed = []       # last typed Latin characters (up to MAXLEN)
muted = set()    # keys "released" by a macro, until the user releases them physically

while True:
    try:
        dev.read(EP_IN, buf, timeout=100)
    except usb.core.USBTimeoutError:
        continue
    except Exception as e:
        print("Read error:", e)
        kbd_out.send_report(bytes(8))
        prev, typed, muted = [], [], set()
        dev = connect()
        continue

    mod = buf[0]
    keys = [k for k in buf[2:8] if k]
    muted = {k for k in muted if k in keys}
    new = [k for k in keys if k not in prev]
    prev = keys

    # -- recognize the sequence of typed characters --
    macro = None
    for kc in new:
        if kc in FILTER_C:
            continue
        if mod & MOD_OTHER:  # Ctrl/Alt/Win -> this is not text
            typed.clear()
            continue
        if kc == BACKSPACE:
            if typed:
                typed.pop()
            continue
        ch = SCAN_BASE.get(kc)  # keycode -> base Latin character
        if ch is None:  # arrows, Esc, F-keys etc. reset the buffer
            typed.clear()
            continue
        typed.append(ch)
        if len(typed) > MAXLEN:
            typed.pop(0)
        for trig, seq in MACROS_C:
            if tuple(typed[-len(trig):]) == trig:
                macro = seq
                break

    # -- sending: first the pressed report itself, then the macro on top --
    kbd_out.send_report(build(buf, muted))

    if macro:
        play(macro)
        typed.clear()
        muted.update(keys)
        kbd_out.send_report(build(buf, muted))
