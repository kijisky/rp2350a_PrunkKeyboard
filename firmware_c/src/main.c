// main.c — dual-role USB keyboard MITM: TinyUSB device stack (native
// USB, RHPORT0) presents a boot-keyboard + debug CDC to the PC;
// TinyUSB host stack (PIO-USB, RHPORT1) reads the physical keyboard.
// Every physical report is forwarded as-is, and in parallel fed into
// the same trigger-matching logic as code.py; on a match, the
// compiled macro (macros.c) is played back to the PC instead of (or
// in addition to) the raw keystrokes.
//
// This is the C equivalent of code.py — see that file's comments
// (and README.md's "Как это работает" section) for the overall
// architecture; this file just does the same job without Python.
#include <string.h>

#include "bsp/board_api.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "debug.h"
#include "keymap.h"
#include "macros.h"

// --- State -----------------------------------------------------------

static uint8_t s_kbd_dev_addr;
static uint8_t s_kbd_instance;
static bool s_kbd_connected;

static uint8_t s_prev_keys[6];               // keys held in the previous report
static uint8_t s_typed[MACROS_MAX_TRIGGER_LEN];  // trailing "what was typed"
static size_t s_typed_len;
static uint8_t s_muted[6];                   // keys a macro "released" early
static size_t s_muted_count;

// --- Sending reports to the PC ---------------------------------------

static void send_kbd_report(uint8_t modifier, const uint8_t keycode[6]) {
    // tud_hid_keyboard_report() just queues the report; TinyUSB can
    // reject it if the previous one hasn't gone out yet (full EP
    // buffer), so retry for a bit instead of silently dropping keys.
    // TinyUSB's signature takes a non-const array (it doesn't modify
    // it) — our own API keeps `const` for callers' benefit, so cast
    // it away only at this one call site.
    for (int attempt = 0; attempt < 1000; attempt++) {
        if (tud_hid_keyboard_report(0, modifier, (uint8_t *)keycode)) return;
        tud_task();
    }
}

static void release_all(void) {
    static const uint8_t zero[6] = {0};
    send_kbd_report(0, zero);
}

// Builds the report actually sent to the PC: same keys as physically
// pressed, except codes in the FILTER set or currently muted by an
// in-progress macro, which are zeroed out — mirrors build() in
// code.py. Modifiers (Ctrl/Alt/Shift/GUI) always pass through
// unfiltered, same as the Python version.
static void send_passthrough(uint8_t modifier, const uint8_t keys[6]) {
    uint8_t out[6];
    for (int i = 0; i < 6; i++) {
        uint8_t kc = keys[i];
        bool hide = false;
        if (kc != 0) {
            hide = macros_is_filtered(kc);
            for (size_t m = 0; !hide && m < s_muted_count; m++) {
                if (s_muted[m] == kc) hide = true;
            }
        }
        out[i] = hide ? 0 : kc;
    }
    send_kbd_report(modifier, out);
}

// Plays a macro's compiled keystroke sequence to the PC — mirrors
// play() in code.py: release everything first (clears any stuck
// Shift), then press+release each step in turn.
static void play_macro(const keystroke_t *seq, size_t len) {
    release_all();
    for (size_t i = 0; i < len; i++) {
        uint8_t keys[6] = {seq[i].keycode, 0, 0, 0, 0, 0};
        send_kbd_report(seq[i].modifier, keys);
        release_all();
    }
}

// --- Processing one report from the physical keyboard -----------------

static bool key_was_in(const uint8_t keys[6], uint8_t kc) {
    for (int i = 0; i < 6; i++) {
        if (keys[i] == kc) return true;
    }
    return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report) {
    const uint8_t *keys = report->keycode;

    // Drop muted codes that the user has now physically released.
    size_t kept = 0;
    for (size_t i = 0; i < s_muted_count; i++) {
        if (key_was_in(keys, s_muted[i])) s_muted[kept++] = s_muted[i];
    }
    s_muted_count = kept;

    // Newly-pressed codes this report (present now, absent before).
    uint8_t newly[6];
    size_t newly_n = 0;
    for (int i = 0; i < 6; i++) {
        uint8_t kc = keys[i];
        if (kc != 0 && !key_was_in(s_prev_keys, kc)) newly[newly_n++] = kc;
    }
    memcpy(s_prev_keys, keys, 6);

    const keystroke_t *macro_seq = NULL;
    size_t macro_len = 0;

    for (size_t i = 0; i < newly_n && macro_seq == NULL; i++) {
        uint8_t kc = newly[i];
        if (macros_is_filtered(kc)) continue;

        if (report->modifier & KEYMAP_MOD_OTHER) {  // Ctrl/Alt/GUI -> not text
            s_typed_len = 0;
            continue;
        }
        if (kc == KEYCODE_BACKSPACE) {
            if (s_typed_len > 0) s_typed_len--;
            continue;
        }
        if (keymap_base_char(kc) == 0) {  // arrows, Esc, F-keys, ... -> not text
            s_typed_len = 0;
            continue;
        }

        if (s_typed_len < MACROS_MAX_TRIGGER_LEN) {
            s_typed[s_typed_len++] = kc;
        } else {
            memmove(s_typed, s_typed + 1, s_typed_len - 1);
            s_typed[s_typed_len - 1] = kc;
        }
        size_t max_trig = macros_max_trigger_len();
        if (max_trig > 0 && s_typed_len > max_trig) {
            memmove(s_typed, s_typed + 1, s_typed_len - 1);
            s_typed_len--;
        }

        macro_seq = macros_match(s_typed, s_typed_len, &macro_len);
    }

    // Forward the physical report first, exactly like code.py does
    // (send the real keypress, then play the macro on top of it).
    send_passthrough(report->modifier, keys);

    if (macro_seq) {
        play_macro(macro_seq, macro_len);
        s_typed_len = 0;
        for (int i = 0; i < 6 && s_muted_count < 6; i++) {
            if (keys[i] != 0) s_muted[s_muted_count++] = keys[i];
        }
        send_passthrough(report->modifier, keys);
    }
}

// --- TinyUSB host HID callbacks ---------------------------------------

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report,
                       uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;

    uint8_t protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (protocol != HID_ITF_PROTOCOL_KEYBOARD) {
        // Not a keyboard interface (could be a boot mouse on the same
        // hub, etc.) — ignore it, but still let other HID devices on
        // the same port enumerate normally.
        return;
    }

    debug_printf("Keyboard connected (addr=%u instance=%u)", dev_addr, instance);
    s_kbd_dev_addr = dev_addr;
    s_kbd_instance = instance;
    s_kbd_connected = true;
    memset(s_prev_keys, 0, sizeof(s_prev_keys));
    s_typed_len = 0;
    s_muted_count = 0;

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        debug_printf("tuh_hid_receive_report failed right after mount");
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (!s_kbd_connected || dev_addr != s_kbd_dev_addr || instance != s_kbd_instance) return;
    debug_printf("Keyboard disconnected");
    s_kbd_connected = false;
    release_all();  // don't leave any key stuck "held" on the PC
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report,
                                 uint16_t len) {
    if (s_kbd_connected && dev_addr == s_kbd_dev_addr && instance == s_kbd_instance &&
        len >= sizeof(hid_keyboard_report_t)) {
        process_kbd_report((hid_keyboard_report_t const *)report);
    }
    // Boot-protocol HID is interrupt-driven: re-arm the transfer or
    // the keyboard goes silent after the very first keystroke.
    tuh_hid_receive_report(dev_addr, instance);
}

// --- Main --------------------------------------------------------------

int main(void) {
    board_init();
    debug_printf("rp2350a_PrunkKeyboard (C firmware) starting");

    macros_setup();

    tud_init(0);   // device role on the native USB controller
    tuh_init(1);   // host role on the PIO-USB controller

    uint32_t last_blink_ms = 0;
    bool led_on = false;

    while (true) {
        tud_task();
        tuh_task();

        // Slow blink while a keyboard is attached, LED off otherwise
        // — the only feedback available on a board with no display.
        uint32_t now = board_millis();
        uint32_t period = s_kbd_connected ? 500 : 0;
        if (period && now - last_blink_ms >= period) {
            last_blink_ms = now;
            led_on = !led_on;
            board_led_write(led_on);
        } else if (!period && led_on) {
            led_on = false;
            board_led_write(false);
        }
    }
}
