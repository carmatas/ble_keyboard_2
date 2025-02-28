#ifndef BLE_HID_KEYBOARD_H
#define BLE_HID_KEYBOARD_H

#include <stdint.h>


extern int32_t current_device_index;
// Initializes BLE HID keyboard
void ble_hid_keyboard_init();

// Sends a key press event
void ble_hid_send_key(uint8_t modifier, uint8_t key_code);

// Releases all keys
void ble_hid_release_key(uint8_t key_code);

void ble_hid_update_modifiers(uint8_t modifier);

void set_next_mac_address();

#endif
