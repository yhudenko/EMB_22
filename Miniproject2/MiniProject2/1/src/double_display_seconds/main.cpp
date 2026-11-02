#include <Arduino.h>
#include <seven_segment_config.h>

#include "display_helper.h"

uint32_t lastValueUpdateMs = 0;
uint32_t lastScanUs = 0;
uint8_t currentValue = 0;
uint8_t activeDisplay = 0;


void setup() {
    Serial.begin(9600);
    delay(1000);

    setPins(kDisplayEnablePins, 2, LOW);
    setPins(kSegmentPins, 7, HIGH);

    pinMode(kDecimalPointPin, OUTPUT);
    digitalWriteReverse(kDecimalPointPin, LOW);

    disableAllDisplays();
}

void loop() {
    const uint32_t nowMs = millis();
    if (nowMs - lastValueUpdateMs >= displayUpdateIntervalMs) {
        lastValueUpdateMs = nowMs;
        currentValue = (currentValue + 1) % 100; // 97
        if (currentValue >= 60) {
            currentValue = 0;
        }
        Serial.println(currentValue);
        // writeSegments(currentValue);
    }

    const uint32_t nowUs = micros();
    if (nowUs - lastScanUs >= digitScanIntervalUs) {
        lastScanUs = nowUs;
        const uint8_t firstDigit = currentValue / 10;
        const uint8_t secondDigit = currentValue % 10;
        const uint8_t activeDigit = (activeDisplay == 0) ? firstDigit : secondDigit;

        showDigit(activeDisplay, activeDigit);

        activeDisplay = (activeDisplay + 1) % 2;

    }
}
