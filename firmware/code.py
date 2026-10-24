"""
code.py — основной цикл устройства.

Архитектура: плата на RP2350A подключена одновременно:
  - к компьютеру через родной USB-разъём платы как USB HID-клавиатура
    (`usb_hid`, настроено в boot.py) — то, что компьютер видит;
  - к физической клавиатуре через второй USB-A разъём платы, который
    работает в режиме USB HOST (`usb.core`, нативная поддержка RP2350
    в CircuitPython 9.x на платах с двумя USB-портами/USB-OTG) —
    оттуда устройство читает "сырые" boot-keyboard отчёты.

Иными словами: физическая клавиатура подключена В плату, плата
подключена В компьютер. Всё, что печатает пользователь, проходит
через этот цикл, который либо пропускает отчёт как есть, либо
подменяет его срабатыванием макроса из config.MACROS.

Это MITM ("человек посередине") на уровне HID-протокола: устройство
полностью контролирует, что компьютер видит из того, что было
набрано. См. README.md, раздел "Этика и границы использования",
прежде чем использовать это на устройстве, которое не принадлежит
вам лично.
"""

import time
import usb.core
import usb_hid

import config
from keymap import BACKSPACE, MOD_OTHER, SCAN_BASE, key_of, norm_trigger

kbd_out = next(d for d in usb_hid.devices if d.usage_page == 0x01 and d.usage == 0x06)

EP_IN = 0x81
buf = bytearray(8)

# Компилируем конфиг один раз при старте — опечатка в config.py
# упадёт сразу при загрузке, а не посреди работы устройства.
MACROS_C = [
    (norm_trigger(trig), [key_of(tok) for tok in seq])
    for trig, seq in config.MACROS.items()
]
MAXLEN = max(len(trig) for trig, _ in MACROS_C) if MACROS_C else 0
FILTER_C = {key_of(tok)[1] for tok in config.FILTER}


def play(seq):
    """Воспроизвести компьютеру последовательность клавиш макроса."""
    kbd_out.send_report(bytes(8))  # отпустить всё, в т.ч. зажатый Shift
    for mod, kc in seq:
        kbd_out.send_report(bytes([mod, 0, kc, 0, 0, 0, 0, 0]))
        kbd_out.send_report(bytes(8))


def build(src, muted):
    """Копия входного отчёта с обнулёнными отфильтрованными/
    приглушёнными кодами клавиш (компьютер их не увидит)."""
    out = bytearray(src)
    for i in range(2, 8):
        if out[i] in FILTER_C or out[i] in muted:
            out[i] = 0
    return out


def connect():
    """Дождаться подключения физической клавиатуры к host-порту и
    перехватить управление у встроенного HID-драйвера CircuitPython
    (который иначе сам пытается её сконфигурировать)."""
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


# === Главный цикл =================================================
dev = connect()
prev = []        # коды, зажатые в прошлом отчёте
typed = []       # последние набранные латинские символы (до MAXLEN)
muted = set()    # клавиши, "отпущенные" макросом, пока их не отпустят руками

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

    # -- распознавание последовательности набранных символов --
    macro = None
    for kc in new:
        if kc in FILTER_C:
            continue
        if mod & MOD_OTHER:  # Ctrl/Alt/Win -> это не текст
            typed.clear()
            continue
        if kc == BACKSPACE:
            if typed:
                typed.pop()
            continue
        ch = SCAN_BASE.get(kc)  # код клавиши -> базовый латинский символ
        if ch is None:  # стрелки, Esc, F-клавиши и т.п. сбрасывают буфер
            typed.clear()
            continue
        typed.append(ch)
        if len(typed) > MAXLEN:
            typed.pop(0)
        for trig, seq in MACROS_C:
            if tuple(typed[-len(trig):]) == trig:
                macro = seq
                break

    # -- отправка: сначала сам нажатый отчёт, потом макрос поверх --
    kbd_out.send_report(build(buf, muted))

    if macro:
        play(macro)
        typed.clear()
        muted.update(keys)
        kbd_out.send_report(build(buf, muted))
