#include "buttons.hpp"
#include "config.hpp"
#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace
{
    const char *TAG = "BUTTONS";

    volatile bool changeDurationPending = false;
    volatile bool playMelodyPending = false;
    volatile bool resetMelodyPending = false;
    portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;

    void onChangeDurationButtonClick(void *arg, void *data)
    {
        ESP_LOGI(TAG, "ChangeDurationButton single click!");
        portENTER_CRITICAL(&buttonMux);
        changeDurationPending = true;
        portEXIT_CRITICAL(&buttonMux);
    }

    void onPlayResetButtonPress(void *arg, void *data)
    {
        ESP_LOGI(TAG, "PlayResetButton single click!");
        portENTER_CRITICAL(&buttonMux);
        playMelodyPending = true;
        portEXIT_CRITICAL(&buttonMux);
    }

    void onPlayResetButtonLongPress(void *arg, void *data)
    {
        ESP_LOGI(TAG, "PlayResetButton long press!");
        portENTER_CRITICAL(&buttonMux);
        resetMelodyPending = true;
        portEXIT_CRITICAL(&buttonMux);
    }
} // namespace

namespace buttons
{
    void init()
    {
         button_config_t btnConfig = {};
        btnConfig.long_press_time = buttonConfig::longPressTimeMs;
        btnConfig.short_press_time = buttonConfig::shortPressTimeMs;

        button_gpio_config_t btnGpioConfig = {};

        // Duration Button configuration
        btnGpioConfig.gpio_num = pinConfig::durationButton;
        btnGpioConfig.active_level = buttonConfig::activeLevel;
        btnGpioConfig.disable_pull = false;

        button_handle_t btn = nullptr;

        ESP_ERROR_CHECK(iot_button_new_gpio_device(&btnConfig, &btnGpioConfig, &btn));
        ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, onChangeDurationButtonClick, NULL));

        // Play/Reset Button configuration
        btnGpioConfig.gpio_num = pinConfig::playResetButton;

        ESP_ERROR_CHECK(iot_button_new_gpio_device(&btnConfig, &btnGpioConfig, &btn));
        ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, onPlayResetButtonPress, NULL));
        ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, onPlayResetButtonLongPress, NULL));
    }

    bool consumeChangeDurationRequest()
    {
        portENTER_CRITICAL(&buttonMux);
        const bool changeDuration = changeDurationPending;
        changeDurationPending = false;
        portEXIT_CRITICAL(&buttonMux);

        return changeDuration;
    }

    bool consumePlayMelodyRequest()
    {
        portENTER_CRITICAL(&buttonMux);
        const bool playMelody = playMelodyPending;
        playMelodyPending = false;
        portEXIT_CRITICAL(&buttonMux);

        return playMelody;
    }

    bool consumeResetMelodyRequest()
    {
        portENTER_CRITICAL(&buttonMux);
        const bool resetMelody = resetMelodyPending;
        resetMelodyPending = false;
        portEXIT_CRITICAL(&buttonMux);

        return resetMelody;
    }
} // namespace buttons