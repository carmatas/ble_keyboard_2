#ifndef BLE_HID_KEYBOARD_H
#define BLE_HID_KEYBOARD_H

#include <stdint.h>

// Initializes BLE HID keyboard
void ble_hid_keyboard_init();

// Sends a key press event
void ble_hid_send_key(uint8_t modifier, uint8_t key_code);

// Releases all keys
void ble_hid_release_key(uint8_t key_code);

#endif
