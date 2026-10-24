// macros.c — edit MACROS[]/FILTER[] below to change what the device
// does; the matching engine beneath that line mirrors code.py and
// should not need to change for a config edit.
#include "macros.h"

#include <string.h>

#include "debug.h"

// =====================================================================
// EDIT HERE — triggers, replacements, filtered keys.
// Direct port of firmware/config.py: same three example groups.
// =====================================================================

// --- Пранки для друзей/коллег (срабатывают всегда) -----------------
static const char *const TOK_privet[] = {"и", "к", "и"};              // привет -> приветики
static const char *const TOK_dokument[] = {"и", "к"};                 // документ -> документик

// --- Исправление типичных опечаток ----------------------------------
static const char *const TOK_oshibka[] = {"backspace", "backspace", "о", "ч", "к", "а"};
static const char *const TOK_hallo[] = {"backspace", "backspace", "o", "l"};
static const char *const TOK_ipconfig[] = {"backspace", "backspace", "g", "i"};

// --- Демонстрация атаки при физическом доступе ----------------------
// См. README.md, раздел "Этика и границы использования", прежде чем
// включать это на устройстве, которое не принадлежит лично вам.
static const char *const TOK_demo_attack[] = {
    "backspace", "backspace", "backspace", "backspace", "backspace", "backspace",
    "н", "е", "х", "о", "р", "о", "ш", "о",
};

static const macro_source_t MACROS[] = {
    {"привет", TOK_privet, 3},
    {"документ", TOK_dokument, 2},
    {"ошибка", TOK_oshibka, 6},
    {"hallo", TOK_hallo, 4},
    {"ipconfig", TOK_ipconfig, 4},
    {"пиздец", TOK_demo_attack, 14},
};
#define MACROS_COUNT (sizeof(MACROS) / sizeof(MACROS[0]))

// Клавиши, которые ПК не увидит вообще (пусто по умолчанию).
static const char *const FILTER[] = {
    // "printscreen",
};
#define FILTER_COUNT (sizeof(FILTER) / sizeof(FILTER[0]))

// =====================================================================
// Движок — сопоставление и выдача макросов. Не редактировать для
// изменения поведения макросов, см. блок выше.
// =====================================================================

static uint8_t g_trigger_codes[MACROS_MAX_COUNT][MACROS_MAX_TRIGGER_LEN];
static size_t g_trigger_len[MACROS_MAX_COUNT];
static keystroke_t g_seq[MACROS_MAX_COUNT][MACROS_MAX_SEQ_LEN];
static size_t g_seq_len[MACROS_MAX_COUNT];
static size_t g_macro_count;
static size_t g_max_trigger_len;

static uint8_t g_filter_codes[MACROS_MAX_FILTER];
static size_t g_filter_count;

static size_t decode_trigger(const char *trigger, uint8_t *out_codes, size_t max_len) {
    size_t n = 0;
    const char *p = trigger;
    while (*p) {
        if (n >= max_len) {
            debug_fatal("trigger '%s' is longer than MACROS_MAX_TRIGGER_LEN (%d)",
                        trigger, (int)max_len);
        }
        keystroke_t ks;
        int consumed = keymap_decode_utf8_char(p, &ks);
        if (consumed == 0) {
            debug_fatal("bad trigger char in '%s' at byte offset %d", trigger,
                        (int)(p - trigger));
        }
        out_codes[n++] = ks.keycode;  // shift is irrelevant for matching
        p += consumed;
    }
    return n;
}

void macros_init(const macro_source_t *table, size_t table_count,
                  const char *const *filter_tokens, size_t filter_count) {
    if (table_count > MACROS_MAX_COUNT) {
        debug_fatal("MACROS has %d entries, max is %d", (int)table_count, MACROS_MAX_COUNT);
    }
    g_macro_count = table_count;
    g_max_trigger_len = 0;

    for (size_t i = 0; i < table_count; i++) {
        const macro_source_t *src = &table[i];

        g_trigger_len[i] = decode_trigger(src->trigger, g_trigger_codes[i], MACROS_MAX_TRIGGER_LEN);
        if (g_trigger_len[i] == 0) {
            debug_fatal("macro #%d has an empty trigger", (int)i);
        }
        if (g_trigger_len[i] > g_max_trigger_len) g_max_trigger_len = g_trigger_len[i];

        if (src->token_count > MACROS_MAX_SEQ_LEN) {
            debug_fatal("macro '%s' replacement has %d tokens, max is %d",
                        src->trigger, (int)src->token_count, MACROS_MAX_SEQ_LEN);
        }
        for (size_t j = 0; j < src->token_count; j++) {
            if (!keymap_resolve_token(src->tokens[j], &g_seq[i][j])) {
                debug_fatal("macro '%s': unknown token '%s'", src->trigger, src->tokens[j]);
            }
        }
        g_seq_len[i] = src->token_count;
    }

    if (filter_count > MACROS_MAX_FILTER) {
        debug_fatal("FILTER has %d entries, max is %d", (int)filter_count, MACROS_MAX_FILTER);
    }
    g_filter_count = filter_count;
    for (size_t i = 0; i < filter_count; i++) {
        keystroke_t ks;
        if (!keymap_resolve_token(filter_tokens[i], &ks)) {
            debug_fatal("FILTER: unknown token '%s'", filter_tokens[i]);
        }
        g_filter_codes[i] = ks.keycode;
    }

    debug_printf("macros_init: %d macros, %d filtered keys, max trigger length %d",
                 (int)g_macro_count, (int)g_filter_count, (int)g_max_trigger_len);
}

size_t macros_max_trigger_len(void) { return g_max_trigger_len; }

bool macros_is_filtered(uint8_t keycode) {
    for (size_t i = 0; i < g_filter_count; i++) {
        if (g_filter_codes[i] == keycode) return true;
    }
    return false;
}

const keystroke_t *macros_match(const uint8_t *typed, size_t typed_len, size_t *out_seq_len) {
    for (size_t i = 0; i < g_macro_count; i++) {
        size_t tlen = g_trigger_len[i];
        if (tlen == 0 || tlen > typed_len) continue;
        const uint8_t *tail = typed + (typed_len - tlen);
        if (memcmp(tail, g_trigger_codes[i], tlen) == 0) {
            *out_seq_len = g_seq_len[i];
            return g_seq[i];
        }
    }
    return NULL;
}

// Entry point used by main.c — keeps the public table/count symbols
// (MACROS/FILTER) private to this file, same separation as
// config.py/code.py.
void macros_setup(void) {
    macros_init(MACROS, MACROS_COUNT, FILTER, FILTER_COUNT);
}
