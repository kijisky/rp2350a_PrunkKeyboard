// debug.h — tiny printf-over-USB-CDC helper.
//
// The device exposes a second USB interface (CDC ACM, "serial port")
// purely for diagnostics — connect to it with any serial terminal
// (115200 baud, though CDC-ACM ignores the actual rate) to see
// "Keyboard connected" / error messages, the same role print()
// played in the CircuitPython version's REPL console.
#pragma once

// printf-style, truncated to a few hundred bytes. Safe to call before
// the host has opened the CDC port — output is simply dropped then.
void debug_printf(const char *fmt, ...);

// Prints `fmt` (with a trailing newline) repeatedly, forever. Used for
// configuration errors in macros_init() that must be visible to
// whoever is developing/building the firmware rather than silently
// limping along with a broken macro table.
void debug_fatal(const char *fmt, ...);
