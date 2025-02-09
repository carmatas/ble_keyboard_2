#include "driver/adc.h"
#include "esp_log.h"

#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_3  // GPIO3 as ADC input
#define ADC_MAX_VALUE 4095                  // 12-bit ADC max value
#define BATTERY_MAX_VOLTAGE 4.2              // Max battery voltage in volts
#define BATTERY_MIN_VOLTAGE 3.0              // Min battery voltage in volts
#define BATTERY_CHANGE_THRESHOLD 5           // Only report if battery changes by 5%

static const char *TAG = "BATTERY";
static int last_battery_percent = -1;  // Store last reported battery level

// 🔹 Get Battery Voltage (returns percentage)
int get_battery_percentage() {
    int adc_reading = adc1_get_raw(BATTERY_ADC_CHANNEL);
    float battery_voltage = ((float)adc_reading / ADC_MAX_VALUE) * BATTERY_MAX_VOLTAGE;

    // Convert voltage to percentage
    int battery_percent = ((battery_voltage - BATTERY_MIN_VOLTAGE) /
                          (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100;

    if (battery_percent > 100) battery_percent = 100; // Cap at 100%
    if (battery_percent < 0) battery_percent = 0;     // Cap at 0%

    return battery_percent;
}

// 🔹 Read battery only if it changed significantly
bool should_send_battery_update() {
    int current_battery = get_battery_percentage();

    // Only update if battery level changed more than 5%
    if (abs(current_battery - last_battery_percent) >= BATTERY_CHANGE_THRESHOLD) {
        last_battery_percent = current_battery;  // Update last value
        return true;
    }
    return false;
}
