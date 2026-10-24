// usb_descriptors.c — what the PC sees: a composite USB device with
// two interfaces:
//   - a boot-protocol HID keyboard (the actual MITM output);
//   - a CDC-ACM "serial port" used only for debug_printf() diagnostics
//     (see debug.h) — open it with any terminal program if you need
//     to see "Keyboard connected"/error messages; the PC does not
//     need it for the device to work as a keyboard.
#include <string.h>

#include "tusb.h"

// --- Device descriptor ---------------------------------------------

#define USB_VID 0xCafe  // TinyUSB's shared test VID/PID range — see
#define USB_PID 0x4012  // README if you need a distinct one for your OS.
#define USB_BCD 0x0200

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    // Composite device (IAD needed because of the CDC interface pair).
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = USB_BCD,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// --- HID report descriptor (boot keyboard) --------------------------

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
    (void)itf;
    return desc_hid_report;
}

// --- Configuration descriptor (HID + CDC) ---------------------------

enum {
    ITF_NUM_HID = 0,
    ITF_NUM_CDC,       // CDC control interface
    ITF_NUM_CDC_DATA,  // CDC data interface
    ITF_NUM_TOTAL,
};

#define EPNUM_HID       0x81
#define EPNUM_CDC_NOTIF 0x82
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x83

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_KEYBOARD,
                        sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8,
                        EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

// --- String descriptors ---------------------------------------------

char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},   // 0: supported language = English (0x0409)
    "RP2350 Prank Keyboard",      // 1: Manufacturer
    "Keyboard Interceptor",       // 2: Product
    "RP2350PK-0001",              // 3: Serial — change per unit if you build several
    "Debug Console",              // 4: CDC interface name
};

static uint16_t _desc_str[32];

// --- Mandatory HID device callbacks ----------------------------------
// TinyUSB requires the application to provide these (they are not
// weak symbols) even for a boot keyboard that never gets a
// GET_REPORT/SET_REPORT request from the host in practice.

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
        if (chr_count > max_count) chr_count = max_count;
        for (size_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = (uint16_t)str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
