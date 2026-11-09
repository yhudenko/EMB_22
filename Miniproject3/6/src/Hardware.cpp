#include "Hardware.hpp"
#include "Config.hpp"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

Button::Button(gpio_num_t pin) : _pin(pin), _lastState(true) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

bool Button::isPressed() {
    bool currentState = gpio_get_level(_pin);
    if (_lastState && !currentState) { 
        vTaskDelay(pdMS_TO_TICKS(50)); 
        _lastState = currentState;
        return true;
    }
    _lastState = currentState;
    return false;
}

Buzzer::Buzzer() {
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_13_BIT;
    timer_cfg.timer_num = LEDC_TIMER_1;
    timer_cfg.freq_hz = 1000;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num = Config::buzzerPin;
    chan_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_cfg.channel = LEDC_CHANNEL_1;
    chan_cfg.timer_sel = LEDC_TIMER_1;
    ledc_channel_config(&chan_cfg);
}

void Buzzer::playTone(uint32_t freq, uint32_t duration_ms) {
    if (freq == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, freq);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 4096); // 50% яскравість/гучність
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void Buzzer::playWin() {
    playTone(523, 150); // Do
    playTone(659, 150); // Mi
    playTone(784, 150); // Sol
    playTone(1046, 300); // Do2
}

void Buzzer::playFail() {
    playTone(293, 300); // Re
    playTone(220, 400); // La
}

ServoLock::ServoLock() {
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_13_BIT;
    timer_cfg.timer_num = LEDC_TIMER_0;
    timer_cfg.freq_hz = 50;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num = Config::servoPin;
    chan_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_cfg.channel = LEDC_CHANNEL_0;
    chan_cfg.timer_sel = LEDC_TIMER_0;
    ledc_channel_config(&chan_cfg);
}

void ServoLock::lock() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void ServoLock::unlock() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 780); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

Potentiometer::Potentiometer() {
    adc_oneshot_unit_init_cfg_t init_cfg = {};
    init_cfg.unit_id = ADC_UNIT_1;
    init_cfg.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
    adc_oneshot_new_unit(&init_cfg, &_adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_12;
    adc_oneshot_config_channel(_adc_handle, ADC_CHANNEL_0, &chan_cfg); // GPIO1 = ADC1_CH0
}

uint32_t Potentiometer::getTimeLimitSeconds() {
    int raw = 0;
    adc_oneshot_read(_adc_handle, ADC_CHANNEL_0, &raw);
    // Масштабуємо від 0..4095 до 10..60 секунд
    return 10 + (raw * 50 / 4095);
}
