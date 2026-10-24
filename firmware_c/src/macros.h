// macros.h — the macro engine: compiles the user-editable trigger
// table (defined in macros.c, in the spot marked "EDIT HERE") into
// keycode sequences, and matches what's being typed against it.
//
// This is the C equivalent of config.py + the MACROS_C/typed-buffer
// matching logic in code.py. Edit the MACROS[] table at the top of
// macros.c to change triggers/replacements/filtered keys — nothing
// here or in main.c needs to change for that.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "keymap.h"

#define MACROS_MAX_COUNT        32
#define MACROS_MAX_TRIGGER_LEN  16
#define MACROS_MAX_SEQ_LEN      32
#define MACROS_MAX_FILTER       16

// One user-editable macro entry. `trigger` is a UTF-8 string (may be
// Cyrillic or Latin, same convention as config.py). `tokens` is the
// replacement, one token per typed/sent key: either a single UTF-8
// character ("и"), or a key name ("backspace", "enter", "f5", ...),
// exactly like the tuples in config.py's MACROS dict.
typedef struct {
    const char *trigger;
    const char *const *tokens;
    size_t token_count;
} macro_source_t;

// Validates and compiles `table` (and `filter_tokens`, a list of key
// tokens to hide from the PC entirely — config.py's FILTER). Call once
// at startup, before the main loop. On a bad trigger/token, prints a
// diagnostic over the debug CDC port and halts — the same "fail at
// startup, not mid-use" behavior as config.py raising on import.
void macros_init(const macro_source_t *table, size_t table_count,
                  const char *const *filter_tokens, size_t filter_count);

// Calls macros_init() with the MACROS[]/FILTER[] table defined at the
// top of macros.c. This is what main.c actually calls at startup.
void macros_setup(void);

// Length (in physical keys) of the longest configured trigger — the
// caller's typed-history ring buffer only needs to keep this many.
size_t macros_max_trigger_len(void);

// True if `keycode` is in the compiled FILTER set (keys hidden from
// the PC outright, unrelated to macro matching).
bool macros_is_filtered(uint8_t keycode);

// Given the most recently typed physical key codes (newest last,
// `typed_len` of them, layout-independent — see keymap_base_char),
// return the matching macro's compiled keystroke sequence, or NULL if
// none of the triggers match the tail of `typed`.
const keystroke_t *macros_match(const uint8_t *typed, size_t typed_len,
                                 size_t *out_seq_len);
