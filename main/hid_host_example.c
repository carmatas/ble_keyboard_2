/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "errno.h"
#include "driver/gpio.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

#include "ble_hid_keyboard.h"
#include "led.h"
#include "esp_system.h"


/* GPIO Pin number for quit from example logic */
#define APP_QUIT_PIN                GPIO_NUM_0

static const char *TAG = "example";

QueueHandle_t app_event_queue = NULL;

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distinguish the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    app_event_group_t event_group;
    /* HID Host - Device related info */
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR    '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial debug output */
#define KEYBOARD_ENTER_LF_EXTEND    1

/**
 * @brief Scancode to ascii table
 */
const uint8_t keycode2ascii [57][2] = {
    {0, 0}, /* HID_KEY_NO_PRESS        */
    {0, 0}, /* HID_KEY_ROLLOVER        */
    {0, 0}, /* HID_KEY_POST_FAIL       */
    {0, 0}, /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'}, /* HID_KEY_A               */
    {'b', 'B'}, /* HID_KEY_B               */
    {'c', 'C'}, /* HID_KEY_C               */
    {'d', 'D'}, /* HID_KEY_D               */
    {'e', 'E'}, /* HID_KEY_E               */
    {'f', 'F'}, /* HID_KEY_F               */
    {'g', 'G'}, /* HID_KEY_G               */
    {'h', 'H'}, /* HID_KEY_H               */
    {'i', 'I'}, /* HID_KEY_I               */
    {'j', 'J'}, /* HID_KEY_J               */
    {'k', 'K'}, /* HID_KEY_K               */
    {'l', 'L'}, /* HID_KEY_L               */
    {'m', 'M'}, /* HID_KEY_M               */
    {'n', 'N'}, /* HID_KEY_N               */
    {'o', 'O'}, /* HID_KEY_O               */
    {'p', 'P'}, /* HID_KEY_P               */
    {'q', 'Q'}, /* HID_KEY_Q               */
    {'r', 'R'}, /* HID_KEY_R               */
    {'s', 'S'}, /* HID_KEY_S               */
    {'t', 'T'}, /* HID_KEY_T               */
    {'u', 'U'}, /* HID_KEY_U               */
    {'v', 'V'}, /* HID_KEY_V               */
    {'w', 'W'}, /* HID_KEY_W               */
    {'x', 'X'}, /* HID_KEY_X               */
    {'y', 'Y'}, /* HID_KEY_Y               */
    {'z', 'Z'}, /* HID_KEY_Z               */
    {'1', '!'}, /* HID_KEY_1               */
    {'2', '@'}, /* HID_KEY_2               */
    {'3', '#'}, /* HID_KEY_3               */
    {'4', '$'}, /* HID_KEY_4               */
    {'5', '%'}, /* HID_KEY_5               */
    {'6', '^'}, /* HID_KEY_6               */
    {'7', '&'}, /* HID_KEY_7               */
    {'8', '*'}, /* HID_KEY_8               */
    {'9', '('}, /* HID_KEY_9               */
    {'0', ')'}, /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0}, /* HID_KEY_ESC             */
    {'\b', 0}, /* HID_KEY_DEL             */
    {0, 0}, /* HID_KEY_TAB             */
    {' ', ' '}, /* HID_KEY_SPACE           */
    {'-', '_'}, /* HID_KEY_MINUS           */
    {'=', '+'}, /* HID_KEY_EQUAL           */
    {'[', '{'}, /* HID_KEY_OPEN_BRACKET    */
    {']', '}'}, /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'}, /* HID_KEY_BACK_SLASH      */
    {'\\', '|'}, /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'}, /* HID_KEY_COLON           */
    {'\'', '"'}, /* HID_KEY_QUOTE           */
    {'`', '~'}, /* HID_KEY_TILDE           */
    {',', '<'}, /* HID_KEY_LESS            */
    {'.', '>'}, /* HID_KEY_GREATER         */
    {'/', '?'} /* HID_KEY_SLASH           */
};

// Add these near top with other global variables
static hid_host_device_handle_t s_keyboard_handle = NULL;
static bool caps_lock = false;
static bool num_lock = false;
static bool scroll_lock = false;

// LED bitmask definitions
#define HID_LED_NUM_LOCK     (1 << 0)
#define HID_LED_CAPS_LOCK    (1 << 1)
#define HID_LED_SCROLL_LOCK  (1 << 2)


#define RESET_BUTTON_GPIO GPIO_NUM_7  // Change GPIO as needed

void configure_reset_button() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RESET_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // Detect button press
    };
    gpio_config(&io_conf);
}

void check_reset_button_task(void *pvParameter) {
    while (1) {
        if (gpio_get_level(RESET_BUTTON_GPIO) == 0) {  // Button pressed
            ESP_LOGI("RESET", "Button Pressed! Saving nex mac to memory and Restarting ESP32...");
            set_next_mac_address() ;
            vTaskDelay(pdMS_TO_TICKS(100)); // Small debounce delay
            esp_restart();  // Force restart
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Poll every 100ms
    }
}

//static uint8_t find_led_report_id(const uint8_t *desc, size_t len) {
//    uint8_t report_id = 0;
//    for (size_t i = 0; i < len; ) {
//        uint8_t tag = desc[i] & 0xFC;   // Get item type
//        uint8_t size_code = desc[i] & 0x03;
//        uint8_t size = (size_code == 3) ? 4 : size_code + 1;
//
//        if (i + size > len) break;  // Prevent out-of-bounds access
//
//        if (tag == 0x04) {  // Usage Page (Global Item)
//            uint32_t usage_page = 0;
//            memcpy(&usage_page, &desc[i+1], size-1);
//
//            if (usage_page == 0x08) {  // 0x08 for LEDs
//                ESP_LOGI(TAG, "✅ LED Usage Page found!");
//            }
//        }
//
//        if (tag == 0x85 && size == 2) {  // Report ID (0x85)
//            report_id = desc[i+1];
//            ESP_LOGI(TAG, "✅ Found Report ID: 0x%02X", report_id);
//            break;  // Stop once we find the report ID
//        }
//
//        i += size;
//    }
//
//    return report_id;
//}
// Add this function to handle LED updates
static void update_leds() {
    if (s_keyboard_handle == NULL) {
        ESP_LOGW(TAG, " No keyboard handle available!");
        return;
    }

    uint8_t report_data = 0;
    report_data |= caps_lock ? HID_LED_CAPS_LOCK : 0;
    report_data |= num_lock ? HID_LED_NUM_LOCK : 0;
    report_data |= scroll_lock ? HID_LED_SCROLL_LOCK : 0;

//    static uint8_t report_id = 0;
//    if (report_id == 0) {
//        // Find the report ID if not set yet
//        size_t desc_len;
//        const uint8_t *report_desc = hid_host_get_report_descriptor(s_keyboard_handle, &desc_len);
//        if (report_desc) {
//            report_id = find_led_report_id(report_desc, desc_len);
//        }
//    }




    //  Clear USB endpoint before sending LED update
    hid_host_dev_params_t dev_params;
    if (ESP_OK == hid_host_device_get_params(s_keyboard_handle, &dev_params)) {
        usb_device_handle_t usb_dev_handle;
        if (usb_host_device_open(NULL, dev_params.addr, &usb_dev_handle) == ESP_OK) {
            usb_host_endpoint_clear(usb_dev_handle, 0); // Reset endpoint 0
            usb_host_device_close(NULL, usb_dev_handle);
        }
    }

    //  Send the LED control report
    esp_err_t err = hid_class_request_set_report(
        s_keyboard_handle,
        HID_REPORT_TYPE_OUTPUT,
        0,  // Report ID (Caps Lock LED)
        &report_data,
        sizeof(report_data)
    );

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ Keyboard LED updated: 0x%02X", report_data);
    } else {
        ESP_LOGE(TAG, "❌ Failed to set LEDs: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto)
{
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) {
            printf("Mouse\r\n");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        } else {
            printf("Generic\r\n");
        }
        fflush(stdout);
    }
}

/**
 * @brief HID Keyboard modifier verification for capitalization application (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier)
{
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) ||
            ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier,
                                         uint8_t key_code,
                                         unsigned char *key_char)
{
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_Z)) {
        // Apply Caps Lock only to alphabetic keys
        if (caps_lock) {
            mod = !mod; // Invert shift effect
        }
        *key_char = keycode2ascii[key_code][mod];
    } else {
        // Other keys (numbers & symbols)
        *key_char = keycode2ascii[key_code][mod];
    }

    return (*key_char != 0);
}

/**
 * @brief HID Keyboard print char symbol
 *
 * @param[in] key_char  Keyboard char to stdout
 */
static inline void hid_keyboard_print_char(unsigned int key_char)
{
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
            putchar('\n');
        }
#endif // KEYBOARD_ENTER_LF_EXTEND
        fflush(stdout);
    }
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t *key_event)
{
    unsigned char key_char;

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (key_event->state == KEY_STATE_PRESSED) {
        // Handle lock keys
        switch (key_event->key_code) {
            case HID_KEY_CAPS_LOCK:
                caps_lock = !caps_lock;
                update_leds();
                break;
            case HID_KEY_NUM_LOCK:
                num_lock = !num_lock;
                update_leds();
                break;
            case HID_KEY_SCROLL_LOCK:
                scroll_lock = !scroll_lock;
                update_leds();
                break;
            default:
                if (hid_keyboard_get_char(key_event->modifier,
                                        key_event->key_code, &key_char)) {
                    hid_keyboard_print_char(key_char);
                }
                break;
        }
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length) {
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
    static uint8_t prev_modifier = 0;
    key_event_t key_event;

    // ✅ **Handle Modifier Keys First**
    if (kb_report->modifier.val != prev_modifier) {
        uint8_t modifier_diff = kb_report->modifier.val ^ prev_modifier; // Find changed modifiers
        for (int i = 0; i < 8; i++) {
            if (modifier_diff & (1 << i)) {
                key_event.modifier = (1 << i);
                key_event.key_code = 0; // Modifiers have no key codes
                key_event.state = (kb_report->modifier.val & (1 << i)) ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
                key_event_callback(&key_event);

                if (key_event.state == KEY_STATE_PRESSED) {
                    ble_hid_update_modifiers(kb_report->modifier.val);
                } else {
                    ble_hid_update_modifiers(kb_report->modifier.val);
                }
            }
        }
        prev_modifier = kb_report->modifier.val;
    }

    // ✅ **Handle Normal Key Presses & Releases**
    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        // **Key Released**
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0; // Modifiers handled separately
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);

            // ✅ **Update BLE HID**
            ble_hid_release_key(key_event.key_code);
        }

        // **Key Pressed**
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);

            // ✅ **Update BLE HID**
            ble_hid_send_key(key_event.modifier, key_event.key_code);
        }
    }

    // ✅ **Update Previous Key State**
    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}


/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t *const data, const int length)
{
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 0;
    static int y_pos = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    printf("X: %06d\tY: %06d\t|%c|%c|\r",
           x_pos, y_pos,
           (mouse_report->buttons.button1 ? 'o' : ' '),
           (mouse_report->buttons.button2 ? 'o' : ' '));
    fflush(stdout);
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t *const data, const int length)
{
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i = 0; i < length; i++) {
        printf("%02X", data[i]);
    }
    putchar('\r');
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                hid_host_keyboard_report_callback(data, data_length);
            } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                hid_host_mouse_report_callback(data, data_length);
            }
        } else {
            hid_host_generic_report_callback(data, data_length);
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                     hid_proto_name_str[dev_params.proto]);
            if (s_keyboard_handle == hid_device_handle) {
                s_keyboard_handle = NULL;
                caps_lock = false;
                num_lock = false;
                scroll_lock = false;
            }
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            break;
//    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
//        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
//                 hid_proto_name_str[dev_params.proto]);
//        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
//        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                 hid_proto_name_str[dev_params.proto]);
        break;
    }
}



/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        // Store keyboard handle when connected
        if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
            s_keyboard_handle = hid_device_handle;
        }

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
            }
        }
        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        break;
    default:
        break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief BOOT button pressed callback
 *
 * Signal application to exit the HID Host task
 *
 * @param[in] arg Unused
 */
static void gpio_isr_cb(void *arg)
{
    BaseType_t xTaskWoken = pdFALSE;
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT,
    };

    if (app_event_queue) {
        xQueueSendFromISR(app_event_queue, &evt_queue, &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT_HID_HOST,
        // HID Host Device related info
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg
    };

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

void app_main(void)
{
    BaseType_t task_created;
    app_event_queue_t evt_queue;
    ESP_LOGI(TAG, "HID Host example");

    // Init BOOT button: Pressing the button simulates app request to exit
    // It will disconnect the USB device and uninstall the HID driver and USB Host Lib
    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(APP_QUIT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_pin));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(APP_QUIT_PIN, gpio_isr_cb, NULL));

    /*
    * Create usb_lib_task to:
    * - initialize USB Host library
    * - Handle USB Host events while APP pin in in HIGH state
    */
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    /*
    * HID host driver configuration
    * - create background task for handling low level event inside the HID driver
    * - provide the device callback to get new HID Device connection event
    */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

//   xTaskCreate(ws2812_blink_task, "LED Task", 4096, NULL, 2, &led_task_handle);

        configure_reset_button();
            xTaskCreate(check_reset_button_task, "reset_task", 2048, NULL, 2, NULL);

    // Initialize BLE HID keyboard
        ble_hid_keyboard_init();

    ESP_LOGI(TAG, "Waiting for HID Device to be connected");

    while (1) {
        // Wait queue
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices == 0) {
                    // End while cycle
                    break;
                } else {
                    ESP_LOGW(TAG, "To shutdown example, remove all USB devices and press button again.");
                    // Keep polling
                }
            }

            if (APP_EVENT_HID_HOST ==  evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                      evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);

             // 🔥 Simulate a Caps Lock Key Press
                 key_event_t fake_caps_lock_event = {
                     .state = KEY_STATE_PRESSED,
                     .modifier = 0,
                     .key_code = HID_KEY_CAPS_LOCK
                 };

                 key_event_callback(&fake_caps_lock_event);

                 // 🔥 Simulate Caps Lock Release
                 vTaskDelay(pdMS_TO_TICKS(100));
                 fake_caps_lock_event.state = KEY_STATE_RELEASED;
                 key_event_callback(&fake_caps_lock_event);

                 ESP_LOGI(TAG, "✅ Simulated Caps Lock event.");
            }
        }
    }

    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    gpio_isr_handler_remove(APP_QUIT_PIN);
    xQueueReset(app_event_queue);
    vQueueDelete(app_event_queue);
}
