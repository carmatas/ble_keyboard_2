#ifndef LED_H
#define LED_H

#include <stdint.h>

extern TaskHandle_t led_task_handle; // ✅ Extern declaration


void start_led();
void ws2812_blink_task(void *pvParameter);
#endif