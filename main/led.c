#include "led_strip.h"
#include "driver/gpio.h"

// #define LED_WS2812_PIN GPIO_NUM_21  // Waveshare ESP32-S3-Zero WS2812 LED Pin
#define LED_WS2812_PIN GPIO_NUM_48  // Waveshare ESP32-S3-Zero WS2812 LED Pin

static led_strip_handle_t led_strip;

void setup_ws2812() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_WS2812_PIN,
        .max_leds = 1, // Only one WS2812 LED on the board
      //  .led_pixel_format = LED_PIXEL_FORMAT_GRB, // WS2812 uses GRB format
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

void set_ws2812_color(uint8_t red, uint8_t green, uint8_t blue) {
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

void start_led(){
        // ✅ Setup WS2812 LED
        setup_ws2812();

        // ✅ Turn on BLUE color at startup
        set_ws2812_color(50, 100, 255);
}