# rp2350a_PrunkKeyboard

A USB keyboard MITM adapter based on the **RP2350A**. The device sits
*between* a physical keyboard and a computer, transparently passes
through every keypress, and when a specified character sequence
appears, it runs a macro (for example, it can erase the text just
typed with a few `backspace` presses and type something else in its
place).

The idea and general architecture are inspired by the project
[daifukusensei/rp2350a-keyboard-interceptor](https://github.com/daifukusensei/rp2350a-keyboard-interceptor).

## What this device is for

- **Pranking friends/colleagues** — for example, when the word "привет"
  is typed, the computer shows "приветики" instead. It is harmless and
  reversible: the device doesn't break anything itself, it just
  substitutes what the computer sees.
- **Demonstrating an attack with physical access** — it visually shows
  a person why they should never give strangers physical access to
  their keyboard or port: a device the size of an adapter can quietly
  rewrite typed text or commands.
- **Personal macros** — auto-correcting common typos (`hallo` → `hol`,
  `ipconfig` → `ipcgi` in the out-of-the-box example — replace these
  with your own), inserting frequently repeated text, and so on.

All three scenarios use the same "trigger → replacement" mechanism and
are configured in a single file (`firmware/config.py`); see the
[“Configuring macros”](#configuring-macros) section.

## Ethics and limits of use

This device physically rewrites what the user types, with no visual
indication on the screen. That is exactly the property that makes such
devices dangerous when connected without the computer owner's consent.

- Use it only on **your own keyboard/PC**, or on a friend's or
  colleague's device **with their explicit consent** — including for
  pranks: tell them it was a joke after the demonstration.
- If you are demonstrating a security vulnerability, do it openly, as
  a teaching example ("look what a stranger could do if they got
  physical access to your port"), not covertly.
- Do not use it at other people's workstations, in other people's
  organizations, or on any device you accessed without the owner's
  permission — this can be a direct violation of the law (unauthorized
  access to computer information, unauthorized modification of data),
  regardless of your intentions.
- The `пиздец` macro in `config.py` (`DEMO_ATTACK_MACROS`) is an
  example specifically for demonstrating an attack (it erases what was
  typed and substitutes different text on a non-obvious trigger).
  Delete or replace it if the device is meant only for everyday macros
  and pranks.

## How it works

1. The physical keyboard is plugged into the board's USB-A connector,
   which operates in **USB host** mode.
2. The board reads raw boot-keyboard HID reports from the keyboard via
   `usb.core` (USB host support built into CircuitPython 9.x on boards
   with two USB ports).
3. The same board is connected to the computer through its native USB
   port and presents itself to the computer as an ordinary USB HID
   keyboard (`usb_hid`).
4. Each incoming report is forwarded to the computer as is, and at the
   same time its keycodes are decoded into text. The matching is
   layout-independent: it is done by physical key code rather than by
   character, so a trigger fires both in the Russian and in the English
   layout selected on the PC.
5. If the last few typed characters match one of the triggers in
   `config.py`, the device plays back the specified key sequence (which
   may include `backspace`), as if the user had typed it themselves.

## What you need to build it

### Hardware

- An **RP2350A** board with support for **two USB roles at the same
  time**: one port in device mode (to the PC), the other in host mode
  (to the keyboard). The exact topology depends on the board — some
  have two physical USB-A connectors, others have one native USB-OTG
  plus a second port via PIO-USB.
- A USB-A adapter/cable for the keyboard's connector, if your keyboard
  is not USB-A.
- The keyboard you will type on.

### Board firmware

- [CircuitPython](https://circuitpython.org/) **9.x or newer** — for
  your specific board, the version must support `usb.core` (USB host).
  Check your board's page on circuitpython.org — not every
  CircuitPython build includes host mode.

## Installation

1. **Flash CircuitPython.**
   Hold the `BOOTSEL` button on the board and connect it to the
   computer via USB — it will mount as a drive named `RPI-RP2`/`RP2350`.
   Copy the `.uf2` CircuitPython file for your board onto this drive.
   The board will reboot and mount as a `CIRCUITPY` drive.

2. **Copy this repository's firmware files.**
   From the [`firmware/`](firmware/) directory, copy the four files to
   the root of the `CIRCUITPY` drive, next to each other:
   - `boot.py`
   - `code.py`
   - `config.py`
   - `keymap.py`

   ```bash
   cp firmware/boot.py firmware/code.py firmware/config.py firmware/keymap.py /Volumes/CIRCUITPY/
   # The mount point depends on the OS: on Linux it is usually
   # /media/<user>/CIRCUITPY/, on Windows it is a drive letter.
   ```

3. **Reboot the board** (reconnecting it via USB is enough) —
   `boot.py` sets up the keyboard HID descriptor, after which `code.py`
   starts and waits for the physical keyboard to be connected to the
   host port.

4. **Connect the physical keyboard** to the board's host port, and the
   board itself to the computer via its device port. The board's serial
   console (for example via `screen`/PuTTY, or the Serial tab in Mu or
   Thonny) should show `Keyboard connected`.

5. **Test the prank/macro.** Open any text editor on the computer and
   type one of the triggers from `config.py` (by default, for example,
   `привет` or `hallo`). The computer should show the substituted
   result, not what was typed on the physical keyboard.

### Configuring macros

Everything you may want to change is in `firmware/config.py` —
`code.py`/`keymap.py` don't need to be touched:

```python
MACROS = {
    "привет":  tuple("ики"),               # привет -> приветики
    "ошибка":  ("backspace",) * 2 + tuple("очка"),  # ошибка -> ошибочка
}
```

- A dictionary key is the trigger (it can be written in Cyrillic or
  Latin; the matching is done automatically by physical key).
- A value is a tuple of tokens: a single character is typed as is; a
  longer string is a key name (`"backspace"`, `"enter"`, `"tab"`,
  `"left"`, `"f5"`, etc.).
- To make the device completely hide some key from the computer (a
  filter rather than a prank), add its token to `FILTER`.

After changing `config.py`, just save the file to the `CIRCUITPY` drive
again — the board restarts `code.py` automatically. A typo in a trigger
or replacement will be visible right away: the device prints an error
to the serial console and does not start, instead of failing silently
during use.

## Work and debug modes

- **Work mode** (the default, after flashing): `boot.py` exposes only
  the keyboard HID device to the computer.
- **Debug mode**: `boot.py` skips the custom HID setup and leaves
  CircuitPython's default USB devices in place. Interception still runs
  and prints its status to the serial console.

To switch modes, press **Ctrl+Alt+ё** on the keyboard connected to the
board (the key with `` ` `` on the English layout). The board saves the
new mode in `microcontroller.nvm`, restarts, and the combo is not sent
to the computer. The mode is kept across power cycles. Press the combo
again to switch back.

## Repository structure

```
firmware/
  boot.py    — USB HID descriptor setup (runs once at startup)
  code.py    — main loop: reading the keyboard, interception, substitution
  keymap.py  — keycode tables and the JCUKEN (Russian) layout
  config.py  — list of macros/triggers and filters — edit here
```
