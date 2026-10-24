"""
boot.py — выполняется один раз при старте платы, до code.py.

Задача: сконфигурировать USB HID-дескриптор устройства, которое
Pico видит компьютер (host-ПК), — только клавиатура, без лишних
интерфейсов (CDC-консоль можно оставить для отладки через serial,
но HID должен быть ровно один boot-keyboard, чтобы ОС сразу
распознавала устройство как обычную клавиатуру).

boot.py не имеет доступа к USB host API (usb.core) — тот доступен
только в code.py, когда основной USB-стек уже поднят.
"""

import usb_hid

# Стандартный HID report descriptor для клавиатуры (boot keyboard,
# 8-байтовый отчёт: modifier, reserved, 6 keycodes) — это именно то,
# что ожидает code.py при вызове usb_hid.devices и kbd_out.send_report().
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

# Отдаём компьютеру ровно одно HID-устройство — клавиатуру.
usb_hid.enable((keyboard,))
