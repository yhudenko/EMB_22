#include "leds_helper.h"
#include <traffic_lights_config.h>

uint16_t msToSeconds(const uint32_t ms) {
    return ms / 1000;
}

void writeLeds(uint8_t ledStates) {
    // Position like 0b001
    digitalWrite(GREEN_LED_PIN, ledStates & (1 << 0));
    digitalWrite(YELLOW_LED_PIN, ledStates & (1 << 1));
    digitalWrite(RED_LED_PIN, ledStates & (1 << 2));
}