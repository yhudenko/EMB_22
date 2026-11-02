#include "HardwareConfig.hpp"
#include "Gpio.hpp"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
// #include "esp_log.h"

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gptimer.html#creating-and-starting-a-timer

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool digitSwitchPending = false;
static volatile uint8_t animationCounter = 0;
static volatile uint16_t trafficLightHalfTickCounter = 0;
static bool animationTimerRunning = false;
static volatile bool readPotPending = false;

bool consumeDigitSwitchRequest()
{
    portENTER_CRITICAL(&timerMux);
    const bool digitSwitch = digitSwitchPending;
    digitSwitchPending = false;
    portEXIT_CRITICAL(&timerMux);

    return digitSwitch;
}

bool consumeReadPotRequest()
{
    portENTER_CRITICAL(&timerMux);
    const bool readPot = readPotPending;
    readPotPending = false;
    portEXIT_CRITICAL(&timerMux);

    return readPot;
}

uint8_t consumeAnimationStepRequest()
{
    portENTER_CRITICAL(&timerMux);
    const uint8_t count = animationCounter;
    animationCounter = 0;
    portEXIT_CRITICAL(&timerMux);

    return count;
}

uint16_t getTrafficLightHalfPeriodCounter()
{
    return trafficLightHalfTickCounter;
}

gptimer_handle_t digitSwitchTimer;
gptimer_handle_t noValueAnimationTimer;
gptimer_handle_t trafficLightTimer;
gptimer_handle_t readPotTimer;

static bool IRAM_ATTR digitSwitchTimerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    portENTER_CRITICAL_ISR(&timerMux);
    digitSwitchPending = true;
    portEXIT_CRITICAL_ISR(&timerMux);
    return false;
}

static bool IRAM_ATTR noValueAnimationTimerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    portENTER_CRITICAL_ISR(&timerMux);
    animationCounter = animationCounter + 1;
    portEXIT_CRITICAL_ISR(&timerMux);
    return false;
}

static bool IRAM_ATTR trafficLightTimerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    portENTER_CRITICAL_ISR(&timerMux);
    trafficLightHalfTickCounter = (trafficLightHalfTickCounter + 1) % (trafficLightDelayTicks.total() * 2);
    portEXIT_CRITICAL_ISR(&timerMux);
    return false;
}

static bool IRAM_ATTR readPotTimerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    portENTER_CRITICAL_ISR(&timerMux);
    readPotPending = true;
    portEXIT_CRITICAL_ISR(&timerMux);
    return false;
}

const gptimer_config_t timerConfig = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1 * 1000 * 1000,
    .intr_priority = 0,
    .flags = {
        .intr_shared = false,
        .allow_pd = false,
    },
};

gptimer_alarm_config_t createAlarmConfig(uint32_t intervalUs)
{
    gptimer_alarm_config_t name {
        .alarm_count = intervalUs,
        .reload_count = 0,
        .flags = {
        .auto_reload_on_alarm = true,
    };
    return name;
}

gptimer_alarm_config_t digitSwitchTimerAlarmConfig = createAlarmConfig(digitScanIntervalUs);
gptimer_alarm_config_t noValueAnimationTimerAlarmConfig = createAlarmConfig(noValueAnimationPeriodMs * 1000ULL);
gptimer_alarm_config_t trafficLightTimerAlarmConfig = createAlarmConfig(trafficLightHalfTickMs * 1000ULL);
gptimer_alarm_config_t readPotTimerAlarmConfig = createAlarmConfig(readPotUpdateMs * 1000ULL);

gptimer_event_callbacks_t digitSwitchTimerCallbacks = {
    .on_alarm = digitSwitchTimerCallback,
};

gptimer_event_callbacks_t noValueAnimationTimerCallbacks = {
    .on_alarm = noValueAnimationTimerCallback,
};

gptimer_event_callbacks_t trafficLightTimerCallbacks = {
    .on_alarm = trafficLightTimerCallback,
};

gptimer_event_callbacks_t readPotTimerCallbacks = {
    .on_alarm = readPotTimerCallback,
};

void timerInit()
{
    ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &digitSwitchTimer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(digitSwitchTimer, &digitSwitchTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(digitSwitchTimer, &digitSwitchTimerCallbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(digitSwitchTimer));

    ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &noValueAnimationTimer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(noValueAnimationTimer, &noValueAnimationTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(noValueAnimationTimer, &noValueAnimationTimerCallbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(noValueAnimationTimer));

    ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &trafficLightTimer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(trafficLightTimer, &trafficLightTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(trafficLightTimer, &trafficLightTimerCallbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(trafficLightTimer));

    ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &readPotTimer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(readPotTimer, &readPotTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(readPotTimer, &readPotTimerCallbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(readPotTimer));

    ESP_ERROR_CHECK(gptimer_start(digitSwitchTimer));
    ESP_ERROR_CHECK(gptimer_start(trafficLightTimer));
    ESP_ERROR_CHECK(gptimer_start(readPotTimer));
}

void startNoValueAnimationTimer()
{
    if (animationTimerRunning)
        return;

    animationCounter = 1; // start immediately
    gptimer_set_raw_count(noValueAnimationTimer, 0);
    ESP_ERROR_CHECK(gptimer_start(noValueAnimationTimer));
    animationTimerRunning = true;
}

void stopNoValueAnimationTimer()
{
    if (animationTimerRunning == false)
        return;

    ESP_ERROR_CHECK(gptimer_stop(noValueAnimationTimer));
    animationTimerRunning = false;
}

void updateTrafficLightTimerSpeed(uint8_t booster)
{
    // ESP_LOGI("Timer.cpp", "== updateTrafficLightTimerSpeed ==");
    // ESP_LOGI("Timer.cpp", "booster: %d", booster);
    ESP_ERROR_CHECK(gptimer_stop(trafficLightTimer));
    ESP_ERROR_CHECK(gptimer_disable(trafficLightTimer));
    trafficLightTimerAlarmConfig.alarm_count =
        static_cast<uint64_t>(trafficLightHalfTickMs) * 1000ULL * speedDecimalsDivider / booster;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(trafficLightTimer, &trafficLightTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_enable(trafficLightTimer));

    stopNoValueAnimationTimer();
    ESP_ERROR_CHECK(gptimer_disable(noValueAnimationTimer));
    noValueAnimationTimerAlarmConfig.alarm_count =
        static_cast<uint64_t>(noValueAnimationPeriodMs) * 1000ULL * speedDecimalsDivider / booster;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(noValueAnimationTimer, &noValueAnimationTimerAlarmConfig));
    ESP_ERROR_CHECK(gptimer_enable(noValueAnimationTimer));


    ESP_ERROR_CHECK(gptimer_start(trafficLightTimer));
    startNoValueAnimationTimer();
}