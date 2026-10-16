#include <Arduino.h>

namespace {
constexpr uint8_t kSegmentPins[] = {
    10, // A
    11, // B
    6,  // C
    5,  // D
    4,  // E
    9,  // F
    8,  // G
};
constexpr uint8_t kDecimalPointPin = 7;

constexpr uint8_t kDigitPatterns[] = {
    0b00111111, // 0: A B C D E F
    0b00000110, // 1: B C
    0b01011011, // 2: A B D E G
    0b01001111, // 3: A B C D G
    0b01100110, // 4: B C F G
    0b01101101, // 5: A C D F G
    0b01111101, // 6: A C D E F G
    0b00000111, // 7: A B C
    0b01111111, // 8: A B C D E F G
    0b01101111, // 9: A B C D F G
};

constexpr unsigned long kSecondMs = 1000;
constexpr uint8_t kNoDigit = 255;

void writeCommonAnodePin(uint8_t pin, bool on)
{
    digitalWrite(pin, on ? LOW : HIGH);
}

void showDigit(uint8_t digit)
{
    const uint8_t pattern = kDigitPatterns[digit];

    for (size_t segment = 0; segment < 7; ++segment) {
        const bool segmentOn = (pattern & (1U << segment)) != 0;
        writeCommonAnodePin(kSegmentPins[segment], segmentOn);
    }
}
}

void setup()
{
    for (const uint8_t pin : kSegmentPins) {
        pinMode(pin, OUTPUT);
        writeCommonAnodePin(pin, false);
    }

    pinMode(kDecimalPointPin, OUTPUT);
    writeCommonAnodePin(kDecimalPointPin, false);

    showDigit(0);
}

void loop()
{
    static uint8_t lastSecondsDigit = kNoDigit;

    const uint8_t secondsDigit = (millis() / kSecondMs) % 10;

    if (secondsDigit != lastSecondsDigit) {
        lastSecondsDigit = secondsDigit;
        showDigit(secondsDigit);
    }
}