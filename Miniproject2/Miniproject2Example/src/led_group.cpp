#include "led_group.h"

#include "pin_map.h"

void LedGroup::begin() {
    pinMode(pins::kRedLed, OUTPUT);
    pinMode(pins::kYellowLed, OUTPUT);
    pinMode(pins::kGreenLed, OUTPUT);
    write(phases::kMaskNone);
}

void LedGroup::write(uint8_t mask) {
    if (mask == lastMask_) {
        return;
    }
    lastMask_ = mask;
    digitalWrite(pins::kRedLed, (mask & phases::kMaskRed) ? HIGH : LOW);
    digitalWrite(pins::kYellowLed, (mask & phases::kMaskYellow) ? HIGH : LOW);
    digitalWrite(pins::kGreenLed, (mask & phases::kMaskGreen) ? HIGH : LOW);
}
