#include "display_helper.h"

void digitalWriteReverse(uint8_t pin, bool val) {
    digitalWrite(pin, !val);
}

void setupDisplay() {
    for (uint8_t pin: ATTEMPT_LEDS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(DISPLAY_PIN, OUTPUT);
    digitalWrite(DISPLAY_PIN, HIGH);

    for (uint8_t pin: kSegmentPins) {
        pinMode(pin, OUTPUT);
        digitalWriteReverse(pin, LOW);
    }

    showDigit(0);
}

void resetLeds() {
    for (uint8_t pin: ATTEMPT_LEDS) {
        digitalWrite(pin, LOW);
    }
}

void resetDisplay() {
    for (uint8_t pin: kSegmentPins) {
        digitalWriteReverse(pin, LOW);
    }
}

void writeSegments(uint8_t digitsPattern) {
    for (size_t i = 0; i < 7; i++) {
        bool isOn = digitsPattern & (1U << i);
        digitalWriteReverse(kSegmentPins[i], isOn);
    }
}

void showDigit(uint8_t digit) {
    uint8_t digitsPattern = kDigitPatterns[digit];
    writeSegments(digitsPattern);
}