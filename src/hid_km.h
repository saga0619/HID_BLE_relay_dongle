#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define HID_REPORT_SIZE_L 8
#define HID_REPORT_SIZE_M 4


bool hid_keyboard_init(void);

bool hid_keyboard_send_report(uint8_t *report);
bool get_hid_key(uint32_t qt_key, uint8_t *hid_key, uint8_t *modifier);

bool hid_mouse_abs_send(uint8_t buttons, uint16_t x, uint16_t y);
bool hid_mouse_abs_clear(void);
// bool hid_touch_send_report(uint8_t contact_count, uint16_t x_pos, uint16_t y_pos, uint8_t contact_id);
// bool hid_touch_clear_report(void);
// void update_modifiers(uint8_t modifier, bool press, uint8_t *cmodifier);
#endif // HID_KEYBOARD_H