#include "led_strip.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_BRIGHTNESS 5  // Scale from 0 (off) to 255 (full brightness)
#define BLINK_INTERVAL_MS 500  // Blink interval in milliseconds

// #define LED_WS2812_PIN GPIO_NUM_21  // Waveshare ESP32-S3-Zero WS2812 LED Pin
#define LED_WS2812_PIN GPIO_NUM_48  // Waveshare ESP32-S3-Zero WS2812 LED Pin

bool ble_connected = false; // Initially false, will be set true when BLE connects

static led_strip_handle_t led_strip;

extern int current_device_index;


TaskHandle_t led_task_handle = NULL; // Store task handle

void setup_ws2812() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_WS2812_PIN,
        .max_leds = 1, // Only one WS2812 LED on the board
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK || led_strip == NULL) {
        ESP_LOGE("LED", "Failed to initialize WS2812: %s", esp_err_to_name(err));
    }
}

void set_ws2812_color(uint8_t red, uint8_t green, uint8_t blue) {
    if (led_strip == NULL) {
        ESP_LOGE("LED", "Attempted to set LED color, but LED strip is not initialized!");
        return;
    }
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

// ✅ Scale brightness manually
void set_ws2812_brightness(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
    red   = (red   * brightness) / 255;
    green = (green * brightness) / 255;
    blue  = (blue  * brightness) / 255;
    set_ws2812_color(red, green, blue);
}


void ws2812_blink_task(void *pvParameter) {
    while (1) {
        if (!ble_connected) {
            // 🔹 Blink BLUE when not connected
            while (!ble_connected) {  // ✅ Wait efficiently for BLE event
                switch (current_device_index) {
                            case 0:
                                set_ws2812_brightness(0, 0, 255, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                break;
                            case 1:
                                set_ws2812_brightness(255, 255, 0, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                break;
                            case 2:
                                set_ws2812_brightness(255, 255, 255, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                break;
                            default:
                                set_ws2812_brightness(100, 0, 0, LED_BRIGHTNESS);
                }
                vTaskDelay(pdMS_TO_TICKS(BLINK_INTERVAL_MS));
                set_ws2812_color(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(BLINK_INTERVAL_MS));

                // 💤 Efficient wait for BLE event (instead of polling)
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLINK_INTERVAL_MS));
            }
        }

        // 🔹 Connected: Solid GREEN for 10s
//        set_ws2812_brightness(0, 255, 0, LED_BRIGHTNESS);
        switch (current_device_index) {
                                    case 0:
                                        set_ws2812_brightness(0, 0, 255, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                        break;
                                    case 1:
                                        set_ws2812_brightness(255, 255, 0, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                        break;
                                    case 2:
                                        set_ws2812_brightness(255, 255, 255, LED_BRIGHTNESS);  // 🔴 Red for PC 1
                                        break;
                                    default:
                                        set_ws2812_brightness(100, 0, 0, LED_BRIGHTNESS);
                        }
        vTaskDelay(pdMS_TO_TICKS(10000));

        // 🔹 Then turn OFF
        set_ws2812_color(0, 0, 0);

        // 💤 Wait for BLE event (efficient power-saving)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}


void start_led() {
    setup_ws2812();  // ✅ Ensure WS2812 LED is properly initialized

    if (led_strip != NULL) {
        xTaskCreate(ws2812_blink_task, "LED Task", 4096, NULL, 2, &led_task_handle);
        ESP_LOGI("LED", "LED Task started successfully!");
    } else {
        ESP_LOGE("LED", "LED strip initialization failed, task will not start.");
    }
}