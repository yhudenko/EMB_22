#include <Arduino.h>
#include <seven_segment_config.h>

static uint8_t currentDigit = 0;
static uint16_t waitFor = 1000;
uint32_t lastTime = 0;

void digitalWriteReverse(uint8_t pin, bool on)
{
    digitalWrite(pin, on ? LOW : HIGH);
}

void showDigit(uint8_t digit) {
    uint8_t pattern = kDigitPatterns[digit];

    for (size_t pos = 0; pos < 7; ++pos) {
        bool positionOn = pattern & (1U << pos);
        digitalWriteReverse(kSegmentPins[pos], positionOn);
    }
}

void setup() {
    Serial.begin(9600);
    delay(1000);
    for (uint8_t pin: kSegmentPins) {
        pinMode(pin, OUTPUT);
        digitalWriteReverse(pin, LOW);
    }

    pinMode(kDecimalPointPin, OUTPUT);
    digitalWriteReverse(kDecimalPointPin, LOW);

    showDigit(currentDigit);
}

void loop() {
    uint32_t currentMillis = millis();
    if (currentMillis - lastTime >= waitFor) {
        lastTime = currentMillis;
        showDigit(currentDigit);
        currentDigit = (currentDigit + 1) % 10;
    }
}