#include <Arduino.h>

constexpr uint8_t kAdcPin = 4;
constexpr uint32_t kSampleRateHz = 10;
constexpr uint32_t kSerialBaud = 115200;

static_assert(kSampleRateHz > 0 && kSampleRateHz <= 1000,
              "Choose a sample rate from 1 to 1000 Hz; serial output limits throughput.");
constexpr uint32_t kSampleIntervalUs = 1000000UL / kSampleRateHz;
uint32_t lastSampleUs = 0;

void setup()
{
    Serial.begin(kSerialBaud);

    const uint32_t startMs = millis();
    while (!Serial && millis() - startMs < 2000UL) {
        delay(10);
    }

    pinMode(kAdcPin, INPUT);
    analogReadResolution(12); // Raw result: 0..4095
    analogSetPinAttenuation(kAdcPin, ADC_11db);

    Serial.printf("Program 1: analogRead(), GPIO %u, %lu Hz\n",
                  static_cast<unsigned>(kAdcPin),
                  static_cast<unsigned long>(kSampleRateHz));
    lastSampleUs = micros();
}

void loop()
{
    const uint32_t nowUs = micros();
    const uint32_t elapsedUs = nowUs - lastSampleUs;
    if (elapsedUs < kSampleIntervalUs) {
        return;
    }

    lastSampleUs += (elapsedUs / kSampleIntervalUs) * kSampleIntervalUs;

    const int raw = analogRead(kAdcPin);
    // const int voltage_mv = analogReadMilliVolts(kAdcPin);

    Serial.printf("raw:%d\n", raw);
}