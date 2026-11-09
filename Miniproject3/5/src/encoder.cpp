#include "config.hpp"
#include "driver/pulse_cnt.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace
{
    const char *TAG = "ENCODER";

    pcnt_unit_handle_t pcntUnit = NULL;
    pcnt_channel_handle_t pcntChannel0 = NULL;
    pcnt_channel_handle_t pcntChannel1 = NULL;
    portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;

    volatile bool addNotePending = false;

    void onClick(void *arg, void *data)
    {
        ESP_LOGI(TAG, "Button single click!");
        portENTER_CRITICAL(&buttonMux);
        addNotePending = true;
        portEXIT_CRITICAL(&buttonMux);
    }

    void configButton()
    {
        button_config_t btnConfig = {};
        btnConfig.long_press_time = buttonConfig::longPressTimeMs;
        btnConfig.short_press_time = buttonConfig::shortPressTimeMs;

        button_gpio_config_t btnGpioConfig = {};
        btnGpioConfig.gpio_num = pinConfig::encoderButton;
        btnGpioConfig.active_level = buttonConfig::activeLevel;
        btnGpioConfig.disable_pull = false;

        button_handle_t btn = nullptr;
        ESP_ERROR_CHECK(iot_button_new_gpio_device(&btnConfig, &btnGpioConfig, &btn));
        ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, onClick, NULL));
    }
} // namespace

namespace encoder
{
    void init()
    {
        pcnt_unit_config_t unitConfig = {};
        unitConfig.high_limit = encoderConfig::highLimit;
        unitConfig.low_limit = encoderConfig::lowLimit;
        unitConfig.flags.accum_count = true;

        ESP_ERROR_CHECK(pcnt_new_unit(&unitConfig, &pcntUnit));

        pcnt_chan_config_t chanConfig0 = {};

        chanConfig0.edge_gpio_num = encoderConfig::pinA;
        chanConfig0.level_gpio_num = encoderConfig::pinB;

        ESP_ERROR_CHECK(pcnt_new_channel(pcntUnit, &chanConfig0, &pcntChannel0));
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcntChannel0,
                                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_INCREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcntChannel0,
                                                      PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

        pcnt_chan_config_t chanConfig1 = {};
        chanConfig1.edge_gpio_num = encoderConfig::pinB;
        chanConfig1.level_gpio_num = encoderConfig::pinA;

        ESP_ERROR_CHECK(pcnt_new_channel(pcntUnit, &chanConfig1, &pcntChannel1));
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcntChannel1,
                                                     PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcntChannel1,
                                                      PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

        pcnt_glitch_filter_config_t filterConfig = {
            .max_glitch_ns = encoderConfig::maxGlitchNs,
        };
        ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcntUnit, &filterConfig));
        ESP_ERROR_CHECK(pcnt_unit_enable(pcntUnit));
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcntUnit));
        ESP_ERROR_CHECK(pcnt_unit_start(pcntUnit));

        ESP_LOGI(TAG, "PCNT X4 Quadrature Mode Initialized");

        configButton();
    }

    bool consumeAddNoteRequest()
    {
        portENTER_CRITICAL(&buttonMux);
        const bool addNote = addNotePending;
        addNotePending = false;
        portEXIT_CRITICAL(&buttonMux);

        return addNote;
    }

    int getCount()
    {
        int value;
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcntUnit, &value));
        return value / encoderConfig::countPerDetent; // Divide by countPerDetent to get the actual count in quadrature mode
    }

    void reset()
    {
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcntUnit));
    }
} // namespace encoder
