#include "HardwareConfig.hpp"
#include "Gpio.hpp"
#include "esp_adc/adc_oneshot.h"

constexpr uint64_t getPinBitMask(const gpio_num_t *pins, size_t count)
{
    uint64_t mask = 0;
    for (size_t i = 0; i < count; ++i)
    {
        mask |= (1ULL << pins[i]);
    }
    return mask;
}

void turnOffDigits()
{
    for (size_t i = 0; i < digitActivatorPinCount; ++i)
    {
        gpio_set_level(digitActivatorPins[i], digitActivatorInactiveLevel);
    }
}

// https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/peripherals/adc/index.html
adc_oneshot_unit_handle_t adc_handle;
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_2,
    .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};

adc_oneshot_chan_cfg_t channel_config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};

void gpioInit()
{
    const gpio_config_t segmentsConfig = {
        .pin_bit_mask = getPinBitMask(segmentPins, segmentPinCount),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&segmentsConfig);

    const gpio_config_t activatorsConfig = {
        .pin_bit_mask = getPinBitMask(digitActivatorPins, digitActivatorPinCount),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&activatorsConfig);

    const gpio_config_t ledsConfig = {
        .pin_bit_mask = (1ULL << ledPin.Green) | (1ULL << ledPin.Yellow) | (1ULL << ledPin.Red),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&ledsConfig);

    turnOffDigits();

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_5, &channel_config));
}

void activateDigit(uint8_t digit)
{
    gpio_set_level(digitActivatorPins[digit], digitActivatorActiveLevel);
}

void showSegments(uint8_t mask)
{
    for (size_t i = 0; i < segmentPinCount; ++i)
    {
        const bool on = mask & (1u << i);
        gpio_set_level(segmentPins[i], on ? segmentActiveLevel : segmentInactiveLevel);
    }
}

uint16_t readPotValue()
{
    int result = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_5, &result));

    return static_cast<uint16_t>(result);
}