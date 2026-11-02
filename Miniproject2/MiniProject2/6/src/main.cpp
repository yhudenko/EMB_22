#include "Gpio.hpp"
#include "Timer.hpp"
#include "Led.hpp"
#include "HardwareConfig.hpp"
#include "Pot.hpp"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

enum DigitMode
{
    Counter,
    NoValueAnimation,
    Nothing,
};

DigitMode currentDigitMode = Counter;

uint8_t activeDigit = 0;
uint8_t digitPattern[] = {
    0b00000000,
    0b00000000,
};
uint8_t animationStepIndex = 0;

struct TrafficLightLed
{
    Led Red;
    Led Yellow;
    Led Green;
};

TrafficLightLed trafficLightLed;

enum TrafficLightState
{
    Red,
    RedAndYellow,
    Green,
    GreenBlink,
    Yellow,
};

TrafficLightState currentTrafficLightState = Red;

void handleDigitModeForCounter(uint16_t halfTicks)
{
    if (halfTicks <= trafficLightMaxVisibleCounter * 2 && currentDigitMode != Counter)
    {
        stopNoValueAnimationTimer();
        currentDigitMode = Counter;
    }
    else if (halfTicks > trafficLightMaxVisibleCounter * 2 && currentDigitMode != NoValueAnimation)
    {
        startNoValueAnimationTimer();
        animationStepIndex = 0;
        currentDigitMode = NoValueAnimation;
    }
}

void handleDigitModeForNothing()
{
    stopNoValueAnimationTimer();
    currentDigitMode = Nothing;
}

void updateDigitPatterns(uint16_t halfTicks)
{
    switch (currentDigitMode)
    {
    case Counter:
    {
        const uint8_t tens = (halfTicks + 1) / 2 / 10;
        const uint8_t seconds = (halfTicks + 1) / 2 % 10;
        digitPattern[0] = digits[tens];
        digitPattern[1] = halfTicks % 2 == 0 ? digits[seconds] | bit(Dot) : digits[seconds];
    }
    break;
    case NoValueAnimation:
    {
        const uint8_t animationStepsPassed = consumeAnimationStepRequest();
        
        if (animationStepsPassed > 0)
        {
            for (size_t i = 0; i < digitActivatorPinCount; ++i)
            {
                digitPattern[i] = noValueAnimationSteps[i][animationStepIndex];
            }
            animationStepIndex = (animationStepIndex + animationStepsPassed) % animationStepCount[0];
        }
    }
    break;
    case Nothing:
    {
        digitPattern[0] = 0b00000000;
        digitPattern[1] = halfTicks % 2 == 0 ? bit(Dot) : 0b00000000;
    }
    break;
    default:
        break;
    }
}

void updateStateMachine()
{
    const uint16_t halfTicks = getTrafficLightHalfPeriodCounter();

    switch (currentTrafficLightState)
    {
    case Red:
    {
        trafficLightLed.Red.set(LedState::On);
        trafficLightLed.Yellow.set(LedState::Off);
        trafficLightLed.Green.set(LedState::Off);

        if (halfTicks < trafficLightDelayTicks.Red * 2)
        {
            const uint16_t halfTicksLeft = trafficLightDelayTicks.Red * 2 - halfTicks;
            handleDigitModeForCounter(halfTicksLeft);
            updateDigitPatterns(halfTicksLeft);
            break;
        }

        currentTrafficLightState = RedAndYellow;
    }
    break;
    case RedAndYellow:
    {
        trafficLightLed.Red.set(LedState::On);
        trafficLightLed.Yellow.set(LedState::On);
        trafficLightLed.Green.set(LedState::Off);

        if (halfTicks < (trafficLightDelayTicks.Red + trafficLightDelayTicks.RedAndYellow) * 2)
        {
            handleDigitModeForNothing();
            updateDigitPatterns(halfTicks);
            break;
        }

        currentTrafficLightState = Green;
    }
    break;
    case Green:
    {
        trafficLightLed.Red.set(LedState::Off);
        trafficLightLed.Yellow.set(LedState::Off);
        trafficLightLed.Green.set(LedState::On);

        if (halfTicks < (trafficLightDelayTicks.Red + trafficLightDelayTicks.RedAndYellow + trafficLightDelayTicks.Green) * 2)
        {
            const uint16_t halfTicksPassed = halfTicks - (trafficLightDelayTicks.Red + trafficLightDelayTicks.RedAndYellow) * 2;
            const uint16_t greenHalfTicksLeft = trafficLightDelayTicks.Green * 2 - halfTicksPassed;
            handleDigitModeForCounter(greenHalfTicksLeft);
            updateDigitPatterns(greenHalfTicksLeft);
            break;
        }

        currentTrafficLightState = GreenBlink;
    }
    break;
    case GreenBlink:
    {
        trafficLightLed.Red.set(LedState::Off);
        trafficLightLed.Yellow.set(LedState::Off);
        if (halfTicks % 2 == 0)
        {
            trafficLightLed.Green.set(LedState::Off);
        }
        else
        {
            trafficLightLed.Green.set(LedState::On);
        }

        handleDigitModeForNothing();
        updateDigitPatterns(halfTicks);

        if (halfTicks < (trafficLightDelayTicks.Red + trafficLightDelayTicks.RedAndYellow + trafficLightDelayTicks.Green + trafficLightDelayTicks.GreenBlink) * 2)
        {
            break;
        }

        currentTrafficLightState = Yellow;
    }
    break;
    case Yellow:
    {
        trafficLightLed.Red.set(LedState::Off);
        trafficLightLed.Yellow.set(LedState::On);
        trafficLightLed.Green.set(LedState::Off);

        if (halfTicks > trafficLightDelayTicks.Red * 2)
        {
            handleDigitModeForNothing();
            updateDigitPatterns(halfTicks);
            break;
        }

        currentTrafficLightState = Red;
    }
    break;
    default:
        break;
    }
}

extern "C" void app_main()
{
    gpioInit();
    timerInit();
    trafficLightLed.Red.init(ledPin.Red);
    trafficLightLed.Yellow.init(ledPin.Yellow);
    trafficLightLed.Green.init(ledPin.Green);

    while (true)
    {
        updateStateMachine();

        if (consumeReadPotRequest()) {
            updateSpeed();
        }

        if (consumeDigitSwitchRequest())
        {
            turnOffDigits();
            showSegments(digitPattern[activeDigit]);
            activateDigit(activeDigit);
            activeDigit = (activeDigit + 1) % digitActivatorPinCount;
        }

        vTaskDelay(pdMS_TO_TICKS(superloopYieldDelayMs));
    }
}