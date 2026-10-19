#include <Arduino.h>

#include "seven_segment_config.h"

namespace {
constexpr uint8_t kPotentiometerPin = 14;
constexpr uint32_t kValueTimerIntervalUs = 1000000;
constexpr uint32_t kPotSampleIntervalUs = 20000;
constexpr uint32_t kFastestDigitScanIntervalUs = 500;
constexpr uint32_t kSlowestDigitScanIntervalUs = 500000;
constexpr uint16_t kPotDeadband = 8;

hw_timer_t *gValueTimer = nullptr;
hw_timer_t *gScanTimer = nullptr;
hw_timer_t *gPotTimer = nullptr;
portMUX_TYPE gTimerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t gPendingValueSteps = 0;
volatile uint32_t gPendingScanSteps = 0;
volatile uint32_t gPendingPotSamples = 0;

uint8_t gCurrentValue = 0;
uint8_t gActiveDigit = 0;
uint16_t gLastPotValue = 0;
uint32_t gDigitScanIntervalUs = 4000;

void IRAM_ATTR onValueTimer()
{
    portENTER_CRITICAL_ISR(&gTimerMux);
    ++gPendingValueSteps;
    portEXIT_CRITICAL_ISR(&gTimerMux);
}

void IRAM_ATTR onScanTimer()
{
    portENTER_CRITICAL_ISR(&gTimerMux);
    ++gPendingScanSteps;
    portEXIT_CRITICAL_ISR(&gTimerMux);
}

void IRAM_ATTR onPotTimer()
{
    portENTER_CRITICAL_ISR(&gTimerMux);
    ++gPendingPotSamples;
    portEXIT_CRITICAL_ISR(&gTimerMux);
}

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

uint32_t mapPotToScanIntervalUs(uint16_t potValue)
{
    return static_cast<uint32_t>(map(
        potValue,
        0,
        4095,
        static_cast<long>(kFastestDigitScanIntervalUs),
        static_cast<long>(kSlowestDigitScanIntervalUs)));
}

void updateScanTimerInterval(uint32_t intervalUs)
{
    gDigitScanIntervalUs = intervalUs;
    timerAlarmWrite(gScanTimer, gDigitScanIntervalUs, true);
}

void pollPotentiometer()
{
    const uint16_t potValue = analogRead(kPotentiometerPin);
    const uint16_t difference = (potValue > gLastPotValue)
        ? static_cast<uint16_t>(potValue - gLastPotValue)
        : static_cast<uint16_t>(gLastPotValue - potValue);

    if (difference < kPotDeadband) {
        return;
    }

    gLastPotValue = potValue;
    updateScanTimerInterval(mapPotToScanIntervalUs(potValue));
}

uint32_t consumePendingCount(volatile uint32_t &pendingCounter)
{
    portENTER_CRITICAL(&gTimerMux);
    const uint32_t count = pendingCounter;
    pendingCounter = 0;
    portEXIT_CRITICAL(&gTimerMux);
    return count;
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

    gLastPotValue = analogRead(kPotentiometerPin);
    gDigitScanIntervalUs = mapPotToScanIntervalUs(gLastPotValue);

    gValueTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(gValueTimer, &onValueTimer, true);
    timerAlarmWrite(gValueTimer, kValueTimerIntervalUs, true);
    timerAlarmEnable(gValueTimer);

    gScanTimer = timerBegin(1, 80, true);
    timerAttachInterrupt(gScanTimer, &onScanTimer, true);
    timerAlarmWrite(gScanTimer, gDigitScanIntervalUs, true);
    timerAlarmEnable(gScanTimer);

    gPotTimer = timerBegin(2, 80, true);
    timerAttachInterrupt(gPotTimer, &onPotTimer, true);
    timerAlarmWrite(gPotTimer, kPotSampleIntervalUs, true);
    timerAlarmEnable(gPotTimer);
}

void loop()
{
    if (consumePendingCount(gPendingPotSamples) > 0) {
        pollPotentiometer();
    }

    const uint32_t valueSteps = consumePendingCount(gPendingValueSteps);
    if (valueSteps > 0) {
        gCurrentValue = static_cast<uint8_t>((gCurrentValue + valueSteps) % 100);
    }

    if (consumePendingCount(gPendingScanSteps) > 0) {
        const uint8_t tens = gCurrentValue / 10;
        const uint8_t ones = gCurrentValue % 10;
        const uint8_t digitValue = (gActiveDigit == 0) ? tens : ones;

        showDigit(gActiveDigit, digitValue);
        gActiveDigit = (gActiveDigit + 1) % (sizeof(kDigitEnablePins) / sizeof(kDigitEnablePins[0]));
    }
}