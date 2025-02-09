#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

int get_battery_percentage();  // Function to read battery percentage

bool should_send_battery_update();


#endif  // BATTERY_H