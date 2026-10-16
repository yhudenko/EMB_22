#include <Arduino.h>

namespace {
constexpr uint8_t kRgbLedPin = 48;
constexpr uint8_t kPotPin = 1;
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

constexpr int kAdcMin = 0;
constexpr int kAdcMax = 4095;
constexpr uint8_t kBrightness = 48;
constexpr unsigned long kSecondMs = 1000;
constexpr unsigned long kMinLoopDelayMs = 100;
constexpr unsigned long kMaxLoopDelayMs = 3000;
constexpr uint8_t kNoDigit = 255;

struct RgbColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

RgbColor colorWheel(uint8_t position)
{
    if (position < 85) {
        return {static_cast<uint8_t>(255 - position * 3), static_cast<uint8_t>(position * 3), 0};
    }

    if (position < 170) {
        position -= 85;
        return {0, static_cast<uint8_t>(255 - position * 3), static_cast<uint8_t>(position * 3)};
    }

    position -= 170;
    return {static_cast<uint8_t>(position * 3), 0, static_cast<uint8_t>(255 - position * 3)};
}

uint8_t scaleColor(uint8_t value)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * kBrightness) / 255);
}

int readPot()
{
    return constrain(analogRead(kPotPin), kAdcMin, kAdcMax);
}

unsigned long getLoopDelayMs()
{
    return map(readPot(), kAdcMin, kAdcMax, kMinLoopDelayMs, kMaxLoopDelayMs);
}

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

void setDecimalPoint(bool on)
{
    writeCommonAnodePin(kDecimalPointPin, on);
}

void setOnboardRgb(bool on)
{
    if (!on) {
        neopixelWrite(kRgbLedPin, 0, 0, 0);
        return;
    }

    const int potValue = readPot();
    const uint8_t colorPosition = map(potValue, kAdcMin, kAdcMax, 0, 255);
    const RgbColor color = colorWheel(colorPosition);

    neopixelWrite(
        kRgbLedPin,
        scaleColor(color.red),
        scaleColor(color.green),
        scaleColor(color.blue));
}
}

void setup()
{
    analogReadResolution(12);
    for (const uint8_t pin : kSegmentPins) {
        pinMode(pin, OUTPUT);
        writeCommonAnodePin(pin, false);
    }

    pinMode(kDecimalPointPin, OUTPUT);
    setDecimalPoint(false);

    showDigit(0);
    setOnboardRgb(false);
}

void loop()
{
    static uint8_t lastSecondsDigit = kNoDigit;
    static bool blinkOn = false;

    const unsigned long now = millis();
    const uint8_t secondsDigit = (now / kSecondMs) % 10;

    if (secondsDigit != lastSecondsDigit) {
        lastSecondsDigit = secondsDigit;
        blinkOn = !blinkOn;

        showDigit(secondsDigit);
        setDecimalPoint(blinkOn);
        setOnboardRgb(blinkOn);
    }

    delay(getLoopDelayMs());
}