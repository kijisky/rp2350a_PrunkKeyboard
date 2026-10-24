// keymap.h — HID usage codes, layout tables, and token parsing.
//
// C port of firmware/keymap.py (the CircuitPython version). Same idea:
// physical keys are identified by their USB HID boot-keyboard usage
// code, independent of whatever layout is active on the host PC. The
// RU_* tables let macros.c define triggers/replacements using Cyrillic
// (ЙЦУКЕН) letters that resolve to the same physical key as their
// Latin counterpart, exactly like the Python version.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// One step of a macro/trigger: a physical key with its modifier state.
typedef struct {
    uint8_t modifier;  // 0 or KEYMAP_MOD_SHIFT
    uint8_t keycode;   // HID usage code, 0x00 = no key (padding)
} keystroke_t;

#define KEYMAP_MOD_SHIFT 0x02   // left Shift, as used in an 8-byte HID report
#define KEYMAP_MOD_OTHER 0xDD   // Ctrl | Alt | GUI, both sides — "this isn't text"

#define KEYCODE_BACKSPACE 0x2A

// Decode one UTF-8 codepoint at the start of `s` (ASCII or a 2-byte
// Cyrillic letter from the А-Я/а-я/Ё/ё range) into the (modifier,
// keycode) that types it on a US physical layout. Returns the number
// of bytes consumed (1 for ASCII, 2 for Cyrillic) on success, or 0 if
// the character is not a known typable symbol.
int keymap_decode_utf8_char(const char *s, keystroke_t *out);

// HID keycode -> base Latin character with no modifier applied,
// layout-independent (mirrors SCAN_BASE in keymap.py). Returns 0 if
// the key has no "typed base character" (arrows, F-keys, Esc, ...).
char keymap_base_char(uint8_t keycode);

// Key name (case-insensitive, e.g. "backspace", "enter", "f5", "left")
// -> HID keycode, or 0 if the name is not recognized.
uint8_t keymap_name_to_code(const char *name);

// Resolve one macro token exactly like key_of() in the Python version:
// a token that decodes as a single UTF-8 codepoint is typed as that
// character; anything else is looked up by name. Returns false (and
// leaves *out untouched) if the token is not a single UTF-8 codepoint
// AND not a known key name — callers should treat that as a fatal
// config error, same as the Python version raising ValueError at
// import time.
bool keymap_resolve_token(const char *token, keystroke_t *out);
