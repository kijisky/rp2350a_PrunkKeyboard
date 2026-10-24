"""
keymap.py — таблицы раскладки клавиатуры (US HID usage codes <->
символы) и перевод русских букв (ЙЦУКЕН) на физические клавиши.

Выделено из code.py, чтобы основной файл содержал только логику
перехвата/подмены, а не статические таблицы.
"""

# символ -> (код клавиши HID, нужен ли Shift)
CHAR = {}
for i, c in enumerate("abcdefghijklmnopqrstuvwxyz"):
    CHAR[c] = (0x04 + i, False)
    CHAR[c.upper()] = (0x04 + i, True)
for i, (c, s) in enumerate(zip("1234567890", "!@#$%^&*()")):
    CHAR[c] = (0x1E + i, False)
    CHAR[s] = (0x1E + i, True)
for code, c, s in (
    (0x2D, "-", "_"), (0x2E, "=", "+"), (0x2F, "[", "{"), (0x30, "]", "}"),
    (0x31, "\\", "|"), (0x33, ";", ":"), (0x34, "'", '"'), (0x35, "`", "~"),
    (0x36, ",", "<"), (0x37, ".", ">"), (0x38, "/", "?"),
):
    CHAR[c] = (code, False)
    CHAR[s] = (code, True)
CHAR[" "] = (0x2C, False)
CHAR["\n"] = (0x28, False)
CHAR["\t"] = (0x2B, False)

# Русская буква -> латинская клавиша (раскладка ЙЦУКЕН), чтобы
# триггеры/замены, написанные кириллицей, транслировались в коды
# физических клавиш независимо от раскладки, выбранной на ПК.
RU_MAP = {
    "й": "q", "ц": "w", "у": "e", "к": "r", "е": "t", "н": "y", "г": "u",
    "ш": "i", "щ": "o", "з": "p", "х": "[", "ъ": "]", "ф": "a", "ы": "s",
    "в": "d", "а": "f", "п": "g", "р": "h", "о": "j", "л": "k", "д": "l",
    "ж": ";", "э": "'", "я": "z", "ч": "x", "с": "c", "м": "v", "и": "b",
    "т": "n", "ь": "m", "б": ",", "ю": ".", "ё": "`",
}
# Явные пары строчная/заглавная — не полагаемся на .upper() для
# кириллицы (в CircuitPython это не всегда работает предсказуемо).
RU_LOWER = "йцукенгшщзхъфывапролджэячсмитьбюё"
RU_UPPER = "ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ"
for lo, up in zip(RU_LOWER, RU_UPPER):
    _code = CHAR[RU_MAP[lo]][0]
    CHAR[lo] = (_code, False)   # строчная -> без Shift
    CHAR[up] = (_code, True)    # заглавная -> с Shift

# код клавиши -> базовый латинский символ (без Shift), раскладко-
# независимый. По этой таблице распознаётся набранный текст и
# нормализуются триггеры.
SCAN_BASE = {}
for i, c in enumerate("abcdefghijklmnopqrstuvwxyz"):
    SCAN_BASE[0x04 + i] = c
for i, c in enumerate("1234567890"):
    SCAN_BASE[0x1E + i] = c
for code, c in (
    (0x2D, "-"), (0x2E, "="), (0x2F, "["), (0x30, "]"), (0x31, "\\"),
    (0x33, ";"), (0x34, "'"), (0x35, "`"), (0x36, ","), (0x37, "."),
    (0x38, "/"), (0x2C, " "),
):
    SCAN_BASE[code] = c

# Контроль при старте: ни одна буква алфавита не потерялась.
_missing = [c for c in "абвгдежзийклмнопрстуфхцчшщъыьэюяё" if c not in CHAR]
if _missing:
    print("RU MISSING:", _missing)

# Имена "неалфавитных" клавиш (регистр имени не важен).
KEY = {
    "enter": 0x28, "esc": 0x29, "backspace": 0x2A, "tab": 0x2B, "space": 0x2C,
    "capslock": 0x39, "printscreen": 0x46, "scrolllock": 0x47, "pause": 0x48,
    "insert": 0x49, "home": 0x4A, "pageup": 0x4B, "delete": 0x4C, "end": 0x4D,
    "pagedown": 0x4E, "right": 0x4F, "left": 0x50, "down": 0x51, "up": 0x52,
    "numlock": 0x53, "app": 0x65,
}
for i in range(12):
    KEY["f%d" % (i + 1)] = 0x3A + i
    KEY["f%d" % (i + 13)] = 0x68 + i

BACKSPACE = KEY["backspace"]
MOD_SHIFT = 0x22   # LShift | RShift
MOD_OTHER = 0xDD   # Ctrl / Alt / GUI (обе стороны) — не часть текста


def key_of(tok):
    """Токен (из config.MACROS/FILTER) -> (modifier, keycode)."""
    if len(tok) == 1 and tok in CHAR:
        code, shift = CHAR[tok]
        return (0x02 if shift else 0), code
    if tok.lower() in KEY:
        return 0, KEY[tok.lower()]
    raise ValueError("Unknown key: %r" % tok)


def norm_trigger(trig):
    """Строка-триггер -> кортеж латинских базовых символов (как в
    SCAN_BASE). Кириллица и латиница с одной физической клавиши дают
    один и тот же символ, поэтому триггер срабатывает независимо от
    раскладки, с которой печатают на ПК."""
    out = []
    for ch in trig:
        if ch not in CHAR:
            raise ValueError("Bad trigger char: %r" % ch)
        code = CHAR[ch][0]
        base = SCAN_BASE.get(code)
        if base is None:
            raise ValueError("Trigger char not typable: %r" % ch)
        out.append(base)
    return tuple(out)
