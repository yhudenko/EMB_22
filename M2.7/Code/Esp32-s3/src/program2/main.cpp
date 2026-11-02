#include <Arduino.h>
#include <driver/adc.h>
#include <esp_err.h>

constexpr adc1_channel_t kAdcChannel = ADC1_CHANNEL_3;
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

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12)); // Raw result: 0..4095
    ESP_ERROR_CHECK(adc1_config_channel_atten(kAdcChannel, ADC_ATTEN_DB_12));

    Serial.printf("Program 2: adc1_get_raw(), GPIO 4 / ADC1 channel 3, %lu Hz\n",
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

    const int raw = adc1_get_raw(kAdcChannel);
    if (raw < 0) {
        Serial.println("ADC read failed");
        return;
    }
    Serial.printf("raw:%d\n", raw);
}