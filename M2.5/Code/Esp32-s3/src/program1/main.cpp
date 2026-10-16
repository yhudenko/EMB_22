#include <Arduino.h>

namespace {
constexpr uint8_t kRgbLedPin = 48;
constexpr uint8_t kPotPin = 1;
constexpr int kAdcMin = 0;
constexpr int kAdcMax = 4095;
constexpr uint8_t kBrightness = 48;
constexpr unsigned long kMinBlinkDelayMs = 100;
constexpr unsigned long kMaxBlinkDelayMs = 2000;

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

unsigned long getBlinkDelayMs()
{
    return map(readPot(), kAdcMin, kAdcMax, kMaxBlinkDelayMs, kMinBlinkDelayMs);
}
}

void setup()
{
    analogReadResolution(12);
    setOnboardRgb(false);
}

void loop()
{
    static bool ledOn = false;
    static unsigned long lastToggleMs = 0;

    const unsigned long now = millis();
    const unsigned long blinkDelayMs = getBlinkDelayMs();

    if (now - lastToggleMs >= blinkDelayMs) {
        lastToggleMs = now;
        ledOn = !ledOn;
        setOnboardRgb(ledOn);
    }
}