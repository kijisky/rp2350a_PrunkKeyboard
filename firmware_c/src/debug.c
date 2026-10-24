// debug.c — see debug.h.
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "tusb.h"

static void write_line(const char *buf, int len) {
    if (!tud_cdc_connected()) return;
    // tud_cdc_write() just queues into the ring buffer; flush pushes
    // it out on the next USB frame. Both are safe to call from the
    // main loop (single core, no RTOS).
    tud_cdc_write(buf, (uint32_t)len);
    tud_cdc_write_flush();
}

void debug_printf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (len < 0) return;
    if (len > (int)sizeof(buf) - 3) len = (int)sizeof(buf) - 3;
    buf[len++] = '\r';
    buf[len++] = '\n';
    write_line(buf, len);
}

void debug_fatal(const char *fmt, ...) {
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    while (true) {
        tud_task();  // keep servicing USB so the message can actually go out
        debug_printf("FATAL CONFIG ERROR: %s", msg);
        sleep_ms(1000);
    }
}
