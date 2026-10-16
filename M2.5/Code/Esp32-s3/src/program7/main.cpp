#include <Arduino.h>
#include "soc/gpio_struct.h"

namespace {
constexpr uint8_t kPotPin = 1;
constexpr uint8_t kServoPin = 2;

constexpr int kAdcMin = 0;
constexpr int kAdcMax = 4095;
constexpr uint16_t kMinPulseUs = 500;
constexpr uint16_t kMaxPulseUs = 2500;
constexpr uint16_t kPwmFrameUs = 20000;
constexpr uint16_t kTimerTickUs = 20;
constexpr uint16_t kFrameTicks = kPwmFrameUs / kTimerTickUs;
constexpr uint8_t kServoTimerId = 0;
constexpr uint16_t kTimerPrescaler = 80;
constexpr unsigned long kAdcUpdateMs = 20;
constexpr unsigned long kSerialUpdateMs = 500;

hw_timer_t *servoTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint16_t pulseTicks = 1500 / kTimerTickUs;
volatile uint16_t frameTick = 0;

uint16_t pulseUsToTicks(uint16_t pulseUs)
{
    return constrain(pulseUs / kTimerTickUs, kMinPulseUs / kTimerTickUs, kMaxPulseUs / kTimerTickUs);
}

uint16_t readServoPulseUs()
{
    constexpr uint8_t kSampleCount = 8;
    uint32_t total = 0;

    for (uint8_t sample = 0; sample < kSampleCount; ++sample) {
        total += analogRead(kPotPin);
    }

    const int potValue = constrain(static_cast<int>(total / kSampleCount), kAdcMin, kAdcMax);
    return map(potValue, kAdcMin, kAdcMax, kMinPulseUs, kMaxPulseUs);
}

uint8_t pulseUsToAngle(uint16_t pulseUs)
{
    return map(pulseUs, kMinPulseUs, kMaxPulseUs, 0, 180);
}

void setServoHigh()
{
    GPIO.out_w1ts = 1UL << kServoPin;
}

void setServoLow()
{
    GPIO.out_w1tc = 1UL << kServoPin;
}

void IRAM_ATTR onServoTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);

    if (frameTick == 0) {
        setServoHigh();
    }

    if (frameTick == pulseTicks) {
        setServoLow();
    }

    ++frameTick;
    if (frameTick >= kFrameTicks) {
        frameTick = 0;
    }

    portEXIT_CRITICAL_ISR(&timerMux);
}

void setPulseWidth(uint16_t pulseUs)
{
    portENTER_CRITICAL(&timerMux);
    pulseTicks = pulseUsToTicks(pulseUs);
    portEXIT_CRITICAL(&timerMux);
}
}

void setup()
{
    Serial0.begin(115200);
    analogReadResolution(12);

    pinMode(kServoPin, OUTPUT);
    setServoLow();

    servoTimer = timerBegin(kServoTimerId, kTimerPrescaler, true);
    timerAttachInterrupt(servoTimer, &onServoTimer, true);
    timerAlarmWrite(servoTimer, kTimerTickUs, true);
    timerAlarmEnable(servoTimer);
}

void loop()
{
    static unsigned long lastAdcUpdateMs = 0;
    static unsigned long lastSerialUpdateMs = 0;
    static uint16_t currentPulseUs = 1500;

    const unsigned long now = millis();

    if (now - lastAdcUpdateMs >= kAdcUpdateMs) {
        lastAdcUpdateMs = now;
        currentPulseUs = readServoPulseUs();
        setPulseWidth(currentPulseUs);
    }

    if (now - lastSerialUpdateMs >= kSerialUpdateMs) {
        lastSerialUpdateMs = now;
        Serial0.print("servo pulse us: ");
        Serial0.print(currentPulseUs);
        Serial0.print(" angle: ");
        Serial0.println(pulseUsToAngle(currentPulseUs));
    }
}