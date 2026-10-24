# CMakeLists.txt — builds the firmware .uf2 using the Pico SDK's
# bundled TinyUSB "family" build support (the same machinery
# TinyUSB's own examples/dual/host_hid_to_device_cdc example uses for
# a dual USB-role project: native USB device + PIO-USB host).
#
# Requires, before running cmake:
#   - the PICO_SDK_PATH environment variable pointing at a pico-sdk
#     checkout cloned with --recursive (so lib/tinyusb, and inside it
#     Pico-PIO-USB, are present) — see README.md "Сборка вручную".
#   - FAMILY/BOARD cache variables if your board isn't the default
#     (see PRUNK_PIO_USB_DP_PIN below for the host-port pin override).
cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR
        "PICO_SDK_PATH is not set. Clone pico-sdk (git clone --recursive "
        "https://github.com/raspberrypi/pico-sdk) and export PICO_SDK_PATH "
        "to point at it before configuring. See README.md.")
endif()

# TinyUSB's own family build glue — vendored inside pico-sdk, so we
# don't need our own copy or submodule of TinyUSB.
set(PRUNK_TINYUSB_DIR "$ENV{PICO_SDK_PATH}/lib/tinyusb")
include(${PRUNK_TINYUSB_DIR}/hw/bsp/family_support.cmake)

# rp2350 shares the same native USB peripheral and the same TinyUSB
# "rp2040" family port as the RP2040 — FAMILY=rp2040 is correct for
# both chips. BOARD picks pin/LED defaults; raspberry_pi_pico2 (an
# RP2350 board) is the closest stock match. Override either on the
# cmake command line with -DFAMILY=... -DBOARD=... if needed.
if(NOT DEFINED FAMILY)
    set(FAMILY rp2040)
endif()
if(NOT DEFINED BOARD)
    set(BOARD raspberry_pi_pico2)
endif()

project(prunk_keyboard_c C CXX ASM)

family_initialize_project(prunk_keyboard_c ${CMAKE_CURRENT_LIST_DIR})

add_executable(prunk_keyboard_c
    src/main.c
    src/usb_descriptors.c
    src/keymap.c
    src/macros.c
    src/debug.c
)

target_include_directories(prunk_keyboard_c PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# The stock raspberry_pi_pico2/raspberry_pi_pico board.h (meant for
# single-role, device-only examples) doesn't define which RHPORT is
# host vs device — TinyUSB's hw/bsp/rp2040/family.c needs these for
# the dual-role board_init() path (it errors with "BOARD_TUH_RHPORT
# undeclared" otherwise). RHPORT0 = native USB = device (see
# tusb_config.h); RHPORT1 = PIO-USB = host.
target_compile_definitions(prunk_keyboard_c PUBLIC
    BOARD_TUD_RHPORT=0
    BOARD_TUH_RHPORT=1
)

# GPIO number of the PIO-USB host port's D+ pin; D- must be the very
# next GPIO (DP_PIN + 1), that's a Pico-PIO-USB requirement. Default
# (0/1) matches the common PIO-USB convention used by e.g. the
# PrankKeyboard project this firmware is a C rewrite of; the
# Waveshare RP2350-USB-A board instead wires its USB-A host port to
# GP12/GP13 — pass -DPRUNK_PIO_USB_DP_PIN=12 for that board, or
# whatever your own board's schematic uses.
set(PRUNK_PIO_USB_DP_PIN 0 CACHE STRING
    "GPIO of the PIO-USB host port's D+ pin (D- is this + 1)")
target_compile_definitions(prunk_keyboard_c PUBLIC
    PICO_DEFAULT_PIO_USB_DP_PIN=${PRUNK_PIO_USB_DP_PIN}
)

# Sets up the dual device+host USB role build (device on the native
# controller, host on PIO-USB) for a bare-metal (no RTOS) target —
# the same helper TinyUSB's own dual-role examples use.
family_configure_dual_usb_example(prunk_keyboard_c noos)
