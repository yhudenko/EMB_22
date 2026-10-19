#include <Arduino.h>

#include "seven_segment_config.h"

namespace {
constexpr uint8_t kPotentiometerPin = 14;
constexpr uint32_t kNumberUpdateIntervalMs = 1000;
constexpr uint32_t kPotSampleIntervalMs = 25;
constexpr uint32_t kFastestDigitScanIntervalUs = 500;
constexpr uint32_t kSlowestDigitScanIntervalUs = 500000;

uint8_t gCurrentValue = 0;
uint8_t gActiveDigit = 0;
uint32_t gLastValueUpdateMs = 0;
uint32_t gLastPotSampleMs = 0;
uint32_t gLastScanUs = 0;
uint32_t gDigitScanIntervalUs = 4000;

void disableAllDigits()
{
    for (uint8_t pin : kDigitEnablePins) {
        digitalWrite(pin, kDigitEnableOffLevel);
    }
}

void writeSegments(uint8_t digit)
{
    const uint8_t pattern = kDigitPatterns[digit % 10];

    for (size_t segmentIndex = 0; segmentIndex < (sizeof(kSegmentPins) / sizeof(kSegmentPins[0])); ++segmentIndex) {
        const bool segmentOn = (pattern & (1U << segmentIndex)) != 0;
        digitalWrite(kSegmentPins[segmentIndex], segmentOn ? kSegmentOnLevel : kSegmentOffLevel);
    }

    digitalWrite(kDecimalPointPin, kSegmentOffLevel);
}

void showDigit(uint8_t digitIndex, uint8_t digitValue)
{
    disableAllDigits();
    writeSegments(digitValue);
    digitalWrite(kDigitEnablePins[digitIndex], kDigitEnableOnLevel);
}

void updateScanIntervalFromPot(uint16_t potValue)
{
    gDigitScanIntervalUs = map(
        potValue,
        0,
        4095,
        static_cast<long>(kFastestDigitScanIntervalUs),
        static_cast<long>(kSlowestDigitScanIntervalUs));
}

void pollPotentiometer(uint32_t nowMs)
{
    if (nowMs - gLastPotSampleMs < kPotSampleIntervalMs) {
        return;
    }

    gLastPotSampleMs = nowMs;
    updateScanIntervalFromPot(analogRead(kPotentiometerPin));
}
}

void setup()
{
    analogReadResolution(12);

    for (uint8_t pin : kSegmentPins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, kSegmentOffLevel);
    }

    pinMode(kDecimalPointPin, OUTPUT);
    digitalWrite(kDecimalPointPin, kSegmentOffLevel);

    for (uint8_t pin : kDigitEnablePins) {
        pinMode(pin, OUTPUT);
    }

    disableAllDigits();
    updateScanIntervalFromPot(analogRead(kPotentiometerPin));
}

void loop()
{
    const uint32_t nowMs = millis();
    if (nowMs - gLastValueUpdateMs >= kNumberUpdateIntervalMs) {
        gLastValueUpdateMs = nowMs;
        gCurrentValue = (gCurrentValue + 1) % 100;
    }

    pollPotentiometer(nowMs);

    const uint32_t nowUs = micros();
    if (nowUs - gLastScanUs >= gDigitScanIntervalUs) {
        gLastScanUs = nowUs;

        const uint8_t tens = gCurrentValue / 10;
        const uint8_t ones = gCurrentValue % 10;
        const uint8_t digitValue = (gActiveDigit == 0) ? tens : ones;

        showDigit(gActiveDigit, digitValue);
        gActiveDigit = (gActiveDigit + 1) % (sizeof(kDigitEnablePins) / sizeof(kDigitEnablePins[0]));
    }
}