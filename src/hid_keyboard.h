#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define HID_REPORT_SIZE_L 8

bool hid_keyboard_init(void);

bool hid_keyboard_send_report(uint8_t *report);
bool get_hid_key(uint32_t qt_key, uint8_t *hid_key, uint8_t *modifier);
// void update_modifiers(uint8_t modifier, bool press, uint8_t *cmodifier);
#endif // HID_KEYBOARD_H