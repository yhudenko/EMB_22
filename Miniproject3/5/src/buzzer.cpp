#include "buzzer.hpp"
#include "config.hpp"
#include <driver/ledc.h>

namespace buzzer
{
    void init()
    {
        ledc_timer_config_t timerConfig{};
        timerConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        timerConfig.duty_resolution = buzzerConfig::resolutionBits;
        timerConfig.timer_num = buzzerConfig::timer;
        timerConfig.freq_hz = buzzerConfig::timerFrequencyHz;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;

        ledc_timer_config(&timerConfig);

        ledc_channel_config_t channelConfig{};
        channelConfig.gpio_num = buzzerConfig::pin;
        channelConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        channelConfig.channel = buzzerConfig::channel;
        channelConfig.timer_sel = buzzerConfig::timer;
        channelConfig.duty = 0;
        channelConfig.hpoint = 0;
        channelConfig.flags.output_invert = 0;

        ledc_channel_config(&channelConfig);
    }

    void playNote(uint16_t frequency)
    {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, buzzerConfig::timer, frequency);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, buzzerConfig::channel, buzzerConfig::dutyCycle);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, buzzerConfig::channel);
    }

    void stopNote()
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, buzzerConfig::channel, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, buzzerConfig::channel);
    }

} // namespace buzzer