// keymap.c — see keymap.h. Direct C port of firmware/keymap.py.
#include "keymap.h"

#include <string.h>
#include <strings.h>  // strcasecmp

// --- ASCII character -> (keycode, shift) --------------------------

static bool ascii_to_key(unsigned char c, keystroke_t *out) {
    if (c >= 'a' && c <= 'z') {
        out->keycode = 0x04 + (uint8_t)(c - 'a');
        out->modifier = 0;
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        out->keycode = 0x04 + (uint8_t)(c - 'A');
        out->modifier = KEYMAP_MOD_SHIFT;
        return true;
    }
    // "1234567890" -> 0x1E.. ; shifted "!@#$%^&*()" on the same codes.
    static const char digits[] = "1234567890";
    static const char shifted[] = "!@#$%^&*()";
    for (int i = 0; i < 10; i++) {
        if ((char)c == digits[i]) {
            out->keycode = 0x1E + (uint8_t)i;
            out->modifier = 0;
            return true;
        }
        if ((char)c == shifted[i]) {
            out->keycode = 0x1E + (uint8_t)i;
            out->modifier = KEYMAP_MOD_SHIFT;
            return true;
        }
    }
    struct pair { char lo, hi; uint8_t code; };
    static const struct pair punct[] = {
        {'-', '_', 0x2D}, {'=', '+', 0x2E}, {'[', '{', 0x2F}, {']', '}', 0x30},
        {'\\', '|', 0x31}, {';', ':', 0x33}, {'\'', '"', 0x34}, {'`', '~', 0x35},
        {',', '<', 0x36}, {'.', '>', 0x37}, {'/', '?', 0x38},
    };
    for (size_t i = 0; i < sizeof(punct) / sizeof(punct[0]); i++) {
        if ((char)c == punct[i].lo) { out->keycode = punct[i].code; out->modifier = 0; return true; }
        if ((char)c == punct[i].hi) { out->keycode = punct[i].code; out->modifier = KEYMAP_MOD_SHIFT; return true; }
    }
    if (c == ' ') { out->keycode = 0x2C; out->modifier = 0; return true; }
    if (c == '\n') { out->keycode = 0x28; out->modifier = 0; return true; }
    if (c == '\t') { out->keycode = 0x2B; out->modifier = 0; return true; }
    return false;
}

// --- Russian (ЙЦУКЕН) lowercase codepoint -> Latin key it shares ---
// Mirrors RU_MAP in keymap.py. Codepoints are the lowercase forms
// (U+0430-U+044F, plus U+0451 "ё" handled separately below).

static char ru_lower_to_latin(uint32_t cp) {
    switch (cp) {
        case 0x0439: return 'q';  // й
        case 0x0446: return 'w';  // ц
        case 0x0443: return 'e';  // у
        case 0x043A: return 'r';  // к
        case 0x0435: return 't';  // е
        case 0x043D: return 'y';  // н
        case 0x0433: return 'u';  // г
        case 0x0448: return 'i';  // ш
        case 0x0449: return 'o';  // щ
        case 0x0437: return 'p';  // з
        case 0x0445: return '[';  // х
        case 0x044A: return ']';  // ъ
        case 0x0444: return 'a';  // ф
        case 0x044B: return 's';  // ы
        case 0x0432: return 'd';  // в
        case 0x0430: return 'f';  // а
        case 0x043F: return 'g';  // п
        case 0x0440: return 'h';  // р
        case 0x043E: return 'j';  // о
        case 0x043B: return 'k';  // л
        case 0x0434: return 'l';  // д
        case 0x0436: return ';';  // ж
        case 0x044D: return '\''; // э
        case 0x044F: return 'z';  // я
        case 0x0447: return 'x';  // ч
        case 0x0441: return 'c';  // с
        case 0x043C: return 'v';  // м
        case 0x0438: return 'b';  // и
        case 0x0442: return 'n';  // т
        case 0x044C: return 'm';  // ь
        case 0x0431: return ',';  // б
        case 0x044E: return '.';  // ю
        default: return 0;
    }
}

static bool utf8_decode_codepoint(const char *s, uint32_t *cp, int *len) {
    unsigned char b0 = (unsigned char)s[0];
    if (b0 < 0x80) {
        *cp = b0;
        *len = 1;
        return true;
    }
    if ((b0 & 0xE0) == 0xC0) {
        unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0) != 0x80) return false;
        *cp = ((uint32_t)(b0 & 0x1F) << 6) | (b1 & 0x3F);
        *len = 2;
        return true;
    }
    return false;  // 3/4-byte UTF-8 not needed for this project's alphabets
}

int keymap_decode_utf8_char(const char *s, keystroke_t *out) {
    uint32_t cp;
    int len;
    if (!utf8_decode_codepoint(s, &cp, &len)) return 0;

    if (len == 1) {
        return ascii_to_key((unsigned char)cp, out) ? 1 : 0;
    }

    // ё / Ё sit outside the main Cyrillic block (offset +0x50, not +0x20).
    if (cp == 0x0451) { cp = 0x0451; }  // ё (lowercase) — fallthrough below
    bool shift = false;
    uint32_t lower_cp = cp;
    if (cp == 0x0401) { lower_cp = 0x0451; shift = true; }        // Ё -> ё
    else if (cp >= 0x0410 && cp <= 0x042F) { lower_cp = cp + 0x20; shift = true; }  // А-Я -> а-я
    else if (cp >= 0x0430 && cp <= 0x044F) { shift = false; }     // а-я as-is

    char latin = (lower_cp == 0x0451) ? '`' : ru_lower_to_latin(lower_cp);
    if (!latin) return 0;

    keystroke_t base;
    if (!ascii_to_key((unsigned char)latin, &base)) return 0;
    out->keycode = base.keycode;
    out->modifier = shift ? KEYMAP_MOD_SHIFT : 0;
    return len;
}

char keymap_base_char(uint8_t keycode) {
    if (keycode >= 0x04 && keycode <= 0x1D) return (char)('a' + (keycode - 0x04));
    if (keycode >= 0x1E && keycode <= 0x26) return (char)('1' + (keycode - 0x1E));
    if (keycode == 0x27) return '0';
    switch (keycode) {
        case 0x2D: return '-';
        case 0x2E: return '=';
        case 0x2F: return '[';
        case 0x30: return ']';
        case 0x31: return '\\';
        case 0x33: return ';';
        case 0x34: return '\'';
        case 0x35: return '`';
        case 0x36: return ',';
        case 0x37: return '.';
        case 0x38: return '/';
        case 0x2C: return ' ';
        default: return 0;
    }
}

uint8_t keymap_name_to_code(const char *name) {
    struct named_key { const char *name; uint8_t code; };
    static const struct named_key keys[] = {
        {"enter", 0x28}, {"esc", 0x29}, {"backspace", 0x2A}, {"tab", 0x2B},
        {"space", 0x2C}, {"capslock", 0x39}, {"printscreen", 0x46},
        {"scrolllock", 0x47}, {"pause", 0x48}, {"insert", 0x49}, {"home", 0x4A},
        {"pageup", 0x4B}, {"delete", 0x4C}, {"end", 0x4D}, {"pagedown", 0x4E},
        {"right", 0x4F}, {"left", 0x50}, {"down", 0x51}, {"up", 0x52},
        {"numlock", 0x53}, {"app", 0x65},
        {"f1", 0x3A}, {"f2", 0x3B}, {"f3", 0x3C}, {"f4", 0x3D}, {"f5", 0x3E},
        {"f6", 0x3F}, {"f7", 0x40}, {"f8", 0x41}, {"f9", 0x42}, {"f10", 0x43},
        {"f11", 0x44}, {"f12", 0x45},
        {"f13", 0x68}, {"f14", 0x69}, {"f15", 0x6A}, {"f16", 0x6B}, {"f17", 0x6C},
        {"f18", 0x6D}, {"f19", 0x6E}, {"f20", 0x6F}, {"f21", 0x70}, {"f22", 0x71},
        {"f23", 0x72}, {"f24", 0x73},
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (strcasecmp(name, keys[i].name) == 0) return keys[i].code;
    }
    return 0;
}

bool keymap_resolve_token(const char *token, keystroke_t *out) {
    keystroke_t ks;
    int consumed = keymap_decode_utf8_char(token, &ks);
    if (consumed > 0 && token[consumed] == '\0') {
        *out = ks;
        return true;
    }
    uint8_t code = keymap_name_to_code(token);
    if (code != 0) {
        out->keycode = code;
        out->modifier = 0;
        return true;
    }
    return false;
}
