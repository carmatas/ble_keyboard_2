#include "driver/adc.h"

#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_3  // Example: GPIO 3
#define BATTERY_VOLTAGE_DIVIDER 2.0  // Adjust based on your resistor divider
#define ADC_MAX_VALUE 4095
#define ADC_REF_VOLTAGE 3.3  // ESP32 ADC reference voltage

float read_battery_voltage() {
    int adc_raw = adc1_get_raw(BATTERY_ADC_CHANNEL);
    float voltage = (adc_raw * ADC_REF_VOLTAGE) / ADC_MAX_VALUE;
    return voltage * BATTERY_VOLTAGE_DIVIDER;  // Adjust for divider
}

uint8_t battery_percentage(float voltage) {
    float min_voltage = 3.0;  // Empty battery (adjust for your battery)
    float max_voltage = 4.2;  // Full battery (adjust for Li-ion/LiPo)

    if (voltage > max_voltage) return 100;
    if (voltage < min_voltage) return 0;

    return (uint8_t)((voltage - min_voltage) / (max_voltage - min_voltage) * 100);
}

void battery_monitor_task(void *pvParameter) {
    uint8_t prev_battery_level = 255;  // Set to invalid value to ensure the first update
    while (1) {
        float voltage = read_battery_voltage();
        uint8_t battery_level = battery_percentage(voltage);

        // Only send update if battery level changed by ±5% (to reduce BLE traffic)
        if (abs(battery_level - prev_battery_level) >= 5) {
            prev_battery_level = battery_level;
            send_battery_level_ble(battery_level);
            ESP_LOGI("BATTERY", "Battery Level: %d%% (Voltage: %.2fV)", battery_level, voltage);
        }

        vTaskDelay(pdMS_TO_TICKS(60000));  // Sleep for 60 seconds (adjust as needed)
    }
}

//#include "esp_hidd_prf_api.h"
//
//void send_battery_level_ble(uint8_t level) {
//    esp_hidd_send_battery_level(level);
//}
