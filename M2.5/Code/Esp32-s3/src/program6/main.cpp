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
constexpr uint32_t kDisplayPeriodUs = 1000000;
constexpr unsigned long kMinBlinkPeriodMs = 100;
constexpr unsigned long kMaxBlinkPeriodMs = 3000;
constexpr unsigned long kBlinkPeriodStepMs = 50;
constexpr unsigned long kBlinkPeriodHysteresisMs = 100;
constexpr uint8_t kDisplayTimerId = 0;
constexpr uint8_t kBlinkTimerId = 1;
constexpr uint16_t kTimerPrescaler = 80;

hw_timer_t *displayTimer = nullptr;
hw_timer_t *blinkTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t displayTickCount = 0;
volatile uint32_t blinkTickCount = 0;

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
    constexpr uint8_t kSampleCount = 8;
    uint32_t total = 0;

    for (uint8_t sample = 0; sample < kSampleCount; ++sample) {
        total += analogRead(kPotPin);
    }

    return constrain(static_cast<int>(total / kSampleCount), kAdcMin, kAdcMax);
}

unsigned long getBlinkPeriodMs()
{
    const unsigned long rawPeriodMs = map(readPot(), kAdcMin, kAdcMax, kMinBlinkPeriodMs, kMaxBlinkPeriodMs);
    return (rawPeriodMs / kBlinkPeriodStepMs) * kBlinkPeriodStepMs;
}

void IRAM_ATTR onDisplayTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    ++displayTickCount;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onBlinkTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    ++blinkTickCount;
    portEXIT_CRITICAL_ISR(&timerMux);
}

uint32_t takeDisplayTicks()
{
    portENTER_CRITICAL(&timerMux);
    const uint32_t ticks = displayTickCount;
    displayTickCount = 0;
    portEXIT_CRITICAL(&timerMux);
    return ticks;
}

uint32_t takeBlinkTicks()
{
    portENTER_CRITICAL(&timerMux);
    const uint32_t ticks = blinkTickCount;
    blinkTickCount = 0;
    portEXIT_CRITICAL(&timerMux);
    return ticks;
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

void configureBlinkTimer(unsigned long blinkPeriodMs)
{
    timerAlarmDisable(blinkTimer);
    timerAlarmWrite(blinkTimer, blinkPeriodMs * 1000, true);
    timerAlarmEnable(blinkTimer);
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

    displayTimer = timerBegin(kDisplayTimerId, kTimerPrescaler, true);
    timerAttachInterrupt(displayTimer, &onDisplayTimer, true);
    timerAlarmWrite(displayTimer, kDisplayPeriodUs, true);
    timerAlarmEnable(displayTimer);

    blinkTimer = timerBegin(kBlinkTimerId, kTimerPrescaler, true);
    timerAttachInterrupt(blinkTimer, &onBlinkTimer, true);
    configureBlinkTimer(getBlinkPeriodMs());
}

void loop()
{
    static uint8_t secondsDigit = 0;
    static bool blinkOn = false;
    static unsigned long configuredBlinkPeriodMs = getBlinkPeriodMs();

    const unsigned long blinkPeriodMs = getBlinkPeriodMs();
    if (abs(static_cast<long>(blinkPeriodMs) - static_cast<long>(configuredBlinkPeriodMs)) >= kBlinkPeriodHysteresisMs) {
        configuredBlinkPeriodMs = blinkPeriodMs;
        configureBlinkTimer(configuredBlinkPeriodMs);
    }

    const uint32_t displayTicks = takeDisplayTicks();
    if (displayTicks > 0) {
        secondsDigit = (secondsDigit + displayTicks) % 10;
        showDigit(secondsDigit);
    }

    const uint32_t blinkTicks = takeBlinkTicks();
    if ((blinkTicks % 2) != 0) {
        blinkOn = !blinkOn;
        setDecimalPoint(blinkOn);
        setOnboardRgb(blinkOn);
    }
}