"""
config.py — everything you would want to customize is kept here,
separate from the core logic (code.py). After editing this file, just
save it to the board — code.py doesn't need to be touched.

Triggers and replacements in MACROS can be written in Cyrillic or in
Latin — code.py maps everything to physical keys using the JCUKEN
layout, so the device fires regardless of which layout is selected on
the computer at the moment of typing.

Format of a MACROS value — a tuple of tokens:
  - a single character  -> typed as is (Shift is chosen automatically
                           for the required case/symbol);
  - a longer string     -> a key name: "backspace", "enter", "tab",
                           "left", "f5", "delete", etc. (see the KEY
                           dictionary in keymap.py).

A typical macro for "turn a typo into another word" — first several
"backspace" presses (as many as the number of letters to erase from
the end of the already-typed trigger), then the replacement tokens.
"""

# Probability that a macro fires once its trigger is typed.
# 1.0 = always (100%), 0.4 = 40% of cases.
DEFAULT_PROB = 1.0

# Per-trigger probability overrides, e.g. MACRO_PROB = {"привет": 0.4}.
# Triggers not listed here use DEFAULT_PROB.
MACRO_PROB = {}

# --- Pranks for friends/colleagues --------------------------------
# Always fire (100%) once the trigger has been typed in full.
# Example: type "привет" -> the computer shows "приветики".
PRANK_MACROS = {
    "привет":  tuple("ики"),
    "документ": tuple("ик"),
}

# --- Fixing common typos (everyday usefulness of macros) ----------
# Same general mechanism: the trigger is a typo, the replacement is the
# correct word.
TYPO_MACROS = {
    "ошибка":  ("backspace",) * 2 + tuple("очка"),
    "hallo":   ("backspace",) * 2 + tuple("ol"),
    "ipconfig": ("backspace",) * 2 + tuple("gi"),
}

# --- Demonstration of an attack with physical access --------------
# A deliberately aggressive example for a security demo: typing an
# innocent word erases what was already typed and substitutes
# completely different text — it shows that a device inserted between
# the keyboard and the PC can silently rewrite the user's input.
# Use only on devices where you have permission to demonstrate this
# attack (see the README, section "Ethics and limits of use").
DEMO_ATTACK_MACROS = {
    "пиздец": ("backspace",) * 6 + tuple("нехорошо"),
}

# The final dictionary that code.py reads. The three groups above are
# combined here just for clarity — physically it is one trigger ->
# replacement mechanism, and code.py doesn't need to change to enable
# or disable one of the groups (comment out the corresponding line).
MACROS = {}
MACROS.update(PRANK_MACROS)
MACROS.update(TYPO_MACROS)
MACROS.update(DEMO_ATTACK_MACROS)

# Key codes (as they arrive from the physical keyboard) that the device
# must completely hide from the computer — the computer won't see them
# at all, as if they were never pressed. The entries are tokens in the
# same format as in MACROS (a single character or a key name).
# Example: FILTER = {"printscreen"} — the computer won't learn about
# attempts to take a screenshot.
FILTER = set()
