#include <Arduino.h>
#include <driver/ledc.h>
#include <esp_err.h>

#define LED_PIN 18
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_FREQUENCY 1000     // 1 kHz
#define LEDC_RESOLUTION LEDC_TIMER_10_BIT  // 10-bit (0-1023)

void setup() {
    // Налаштування таймера
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_config.duty_resolution = LEDC_RESOLUTION;
    timer_config.timer_num = LEDC_TIMER;
    timer_config.freq_hz = LEDC_FREQUENCY;
    timer_config.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    
    // Налаштування каналу
    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = LED_PIN;
    channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_config.channel = LEDC_CHANNEL;
    channel_config.timer_sel = LEDC_TIMER;
    channel_config.duty = 0;
    channel_config.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void loop() {
    // Fade-in: поступово збільшуємо яскравість з 0% до 100%
    for (int brightness = 0; brightness <= 1023; brightness += 10) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, brightness);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
        delay(20);  // 20 мс затримка між кроками
    }

    delay(500);  // 500 мс пауза при максимальній яскравості

    // Fade-out: поступово зменшуємо яскравість з 100% до 0%
    for (int brightness = 1023; brightness >= 0; brightness -= 10) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, brightness);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
        delay(20);
    }

    delay(1000);  // 1 секунда пауза перед повтором
}