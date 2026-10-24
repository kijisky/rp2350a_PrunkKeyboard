"""
keymap.py — keyboard layout tables (US HID usage codes <-> characters)
and the mapping of Russian letters (JCUKEN layout) to physical keys.

Split out of code.py so the main file contains only the interception
and substitution logic, not static tables.
"""

# character -> (HID keycode, whether Shift is needed)
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

# Russian letter -> Latin key (JCUKEN layout), so that triggers and
# replacements written in Cyrillic are translated into physical key
# codes regardless of the layout selected on the PC.
RU_MAP = {
    "й": "q", "ц": "w", "у": "e", "к": "r", "е": "t", "н": "y", "г": "u",
    "ш": "i", "щ": "o", "з": "p", "х": "[", "ъ": "]", "ф": "a", "ы": "s",
    "в": "d", "а": "f", "п": "g", "р": "h", "о": "j", "л": "k", "д": "l",
    "ж": ";", "э": "'", "я": "z", "ч": "x", "с": "c", "м": "v", "и": "b",
    "т": "n", "ь": "m", "б": ",", "ю": ".", "ё": "`",
}
# Explicit lowercase/uppercase pairs — we don't rely on .upper() for
# Cyrillic (in CircuitPython it doesn't always behave predictably).
RU_LOWER = "йцукенгшщзхъфывапролджэячсмитьбюё"
RU_UPPER = "ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ"
for lo, up in zip(RU_LOWER, RU_UPPER):
    _code = CHAR[RU_MAP[lo]][0]
    CHAR[lo] = (_code, False)   # lowercase -> no Shift
    CHAR[up] = (_code, True)    # uppercase -> with Shift

# keycode -> base Latin character (without Shift), layout-independent.
# This table is used to recognize typed text and to normalize triggers.
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

# Startup check: make sure no letter of the alphabet is missing.
_missing = [c for c in "абвгдежзийклмнопрстуфхцчшщъыьэюяё" if c not in CHAR]
if _missing:
    print("RU MISSING:", _missing)

# Names of "non-alphabetic" keys (the case of the name doesn't matter).
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
MOD_OTHER = 0xDD   # Ctrl / Alt / GUI (both sides) — not part of the text


def key_of(tok):
    """Token (from config.MACROS/FILTER) -> (modifier, keycode)."""
    if len(tok) == 1 and tok in CHAR:
        code, shift = CHAR[tok]
        return (0x02 if shift else 0), code
    if tok.lower() in KEY:
        return 0, KEY[tok.lower()]
    raise ValueError("Unknown key: %r" % tok)


def norm_trigger(trig):
    """Trigger string -> tuple of Latin base characters (as in
    SCAN_BASE). Cyrillic and Latin letters from the same physical key
    produce the same character, so a trigger fires regardless of the
    layout used for typing on the PC."""
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
