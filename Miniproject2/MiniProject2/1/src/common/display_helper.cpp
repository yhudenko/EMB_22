#include "display_helper.h"
#include <Arduino.h>
#include <seven_segment_config.h>

void digitalWriteReverse(uint8_t pin, bool val) {
    digitalWrite(pin, !val);
}

void setPins(const uint8_t *pins, uint8_t n, bool val) {
    for (uint8_t i = 0; i < n; i++) {
        pinMode(pins[i], OUTPUT);
        digitalWrite(pins[i], val);
    }
}

void disableAllDisplays() {
    for (uint8_t pin: kDisplayEnablePins) { digitalWrite(pin, LOW); }
}

void writeSegments(uint8_t digitsPattern) {
    for (size_t i = 0; i < 8; i++) {
        bool isOn = digitsPattern & (1U << i);
        digitalWriteReverse(kSegmentPins[i], isOn);
    }
}

void showDigit(uint8_t activeDisplayIndex, int digit) {
    uint8_t digitsPattern = digit == -1 ? undefinedDigitPattern : kDigitPatterns[digit % 10];
    disableAllDisplays();
    writeSegments(digitsPattern);
    digitalWrite(kDisplayEnablePins[activeDisplayIndex], HIGH);
}
