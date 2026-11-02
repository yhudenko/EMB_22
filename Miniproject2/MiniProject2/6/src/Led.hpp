#ifndef LED_HPP
#define LED_HPP

#include "driver/gpio.h"

enum class LedBlinkMode
{
    Blink,
    On,
    Off
};

enum class LedState
{
    On,
    Off
};

class Led
{
private:
    static constexpr uint32_t ledPinActiveLevel = 1;
    static constexpr uint32_t ledPinInactiveLevel = 0;
    gpio_num_t m_ledPin;
    LedState m_currentState = LedState::Off;

public:
    void init(gpio_num_t ledPin)
    {
        m_ledPin = ledPin;
    }

    void set(LedState state)
    {
        if (state == m_currentState)
            return;

        m_currentState = state;
        if (state == LedState::On)
        {
            gpio_set_level(m_ledPin, ledPinActiveLevel);
        }
        else
        {
            gpio_set_level(m_ledPin, ledPinInactiveLevel);
        }
    }

        LedState get() const
    {
        return m_currentState;
    }
};

#endif