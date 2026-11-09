#include "servo.hpp"
#include "config.hpp"
#include <driver/ledc.h>
#include "esp_log.h"

namespace
{
    const char *TAG = "SERVO";
}

namespace servo
{
    void init()
    {
        ledc_timer_config_t timerConfig{};
        timerConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        timerConfig.duty_resolution = servoConfig::resolutionBits;
        timerConfig.timer_num = servoConfig::timer;
        timerConfig.freq_hz = servoConfig::frequencyHz;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;

        ledc_timer_config(&timerConfig);

        ledc_channel_config_t channelConfig{};
        channelConfig.gpio_num = servoConfig::pin;
        channelConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        channelConfig.channel = servoConfig::channel;
        channelConfig.timer_sel = servoConfig::timer;
        channelConfig.duty = 0;
        channelConfig.hpoint = 0;
        channelConfig.flags.output_invert = 0;

        ledc_channel_config(&channelConfig);
    }

    void setAngle(int16_t angle)
    {
        angle = servoConfig::maxAngle - (angle > servoConfig::maxAngle ? servoConfig::maxAngle : (angle < servoConfig::minAngle ? servoConfig::minAngle : angle));

        uint32_t pulseWidthUs = servoConfig::minPulseWidthUs + ((servoConfig::maxPulseWidthUs - servoConfig::minPulseWidthUs) * angle) / servoConfig::maxAngle;
        uint32_t duty = (pulseWidthUs * (1 << servoConfig::resolutionBits)) / servoConfig::periodUs;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, servoConfig::channel, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, servoConfig::channel);
        ESP_LOGI(TAG, "Angle: %d° → Pulse: %" PRIu32 " us → Duty: %" PRIu32 "", angle, pulseWidthUs, duty);
    }
} // namespace servo