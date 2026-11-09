#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"

namespace helpers
{
    constexpr uint64_t getPinBitMask(const gpio_num_t *pins, size_t count)
    {
        uint64_t mask = 0;
        for (size_t i = 0; i < count; ++i)
        {
            mask |= (1ULL << pins[i]);
        }
        return mask;
    }

    constexpr uint32_t pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
}

namespace pinConfig
{
    constexpr gpio_num_t servo = GPIO_NUM_12;
    constexpr gpio_num_t encoderButton = GPIO_NUM_15;
    constexpr gpio_num_t encoderA = GPIO_NUM_5;
    constexpr gpio_num_t encoderB = GPIO_NUM_6;
    constexpr gpio_num_t buzzer = GPIO_NUM_11;
    constexpr gpio_num_t durationButton = GPIO_NUM_38;
    constexpr gpio_num_t playResetButton = GPIO_NUM_47;
    constexpr gpio_num_t led1 = GPIO_NUM_16;
    constexpr gpio_num_t led2 = GPIO_NUM_17;
    constexpr gpio_num_t led3 = GPIO_NUM_18;
    constexpr gpio_num_t led4 = GPIO_NUM_8;
    constexpr gpio_num_t digitSegmentA = GPIO_NUM_14;
    constexpr gpio_num_t digitSegmentB = GPIO_NUM_21;
    constexpr gpio_num_t digitSegmentC = GPIO_NUM_4;
    constexpr gpio_num_t digitSegmentD = GPIO_NUM_7;
    constexpr gpio_num_t digitSegmentE = GPIO_NUM_9;
    constexpr gpio_num_t digitSegmentF = GPIO_NUM_10;
    constexpr gpio_num_t digitSegmentG = GPIO_NUM_13;
    constexpr gpio_num_t digitActivator1 = GPIO_NUM_1;
    constexpr gpio_num_t digitActivator2 = GPIO_NUM_2;
}

namespace buttonConfig
{
    constexpr uint32_t longPressTimeMs = 1000;
    constexpr uint32_t shortPressTimeMs = 50;
    constexpr int activeLevel = 0;
}

namespace servoConfig
{
    constexpr gpio_num_t pin = pinConfig::servo;
    constexpr ledc_channel_t channel = LEDC_CHANNEL_0;
    constexpr ledc_timer_t timer = LEDC_TIMER_0;
    constexpr uint32_t frequencyHz = 50;
    constexpr ledc_timer_bit_t resolutionBits = LEDC_TIMER_13_BIT;
    constexpr uint32_t minPulseWidthUs = 500;
    constexpr uint32_t maxPulseWidthUs = 2500;
    constexpr uint32_t microsecondsPerSecond = 1000000;
    constexpr uint32_t periodUs = microsecondsPerSecond / frequencyHz;
    constexpr uint16_t minAngle = 0;
    constexpr uint16_t maxAngle = 180;
}

namespace buzzerConfig
{
    constexpr gpio_num_t pin = pinConfig::buzzer;
    constexpr ledc_channel_t channel = LEDC_CHANNEL_1;
    constexpr ledc_timer_t timer = LEDC_TIMER_1;
    constexpr ledc_timer_bit_t resolutionBits = LEDC_TIMER_10_BIT;
    constexpr uint32_t timerFrequencyHz = 440;

    constexpr uint16_t getDutyCycleByBitWidth(ledc_timer_bit_t bitWidth)
    {
        return (1 << bitWidth) / 2; // 50% duty cycle
    }

    constexpr uint16_t dutyCycle = getDutyCycleByBitWidth(resolutionBits);
}

namespace encoderConfig
{
    constexpr gpio_num_t pinA = pinConfig::encoderA;
    constexpr gpio_num_t pinB = pinConfig::encoderB;
    constexpr uint16_t maxGlitchNs = 1000;
    constexpr int highLimit = 10000;
    constexpr int lowLimit = -10000;
    constexpr uint8_t countPerDetent = 4; // Number of counts per detent (click)
}

namespace ledConfig
{
    constexpr gpio_num_t pins[] = {pinConfig::led1, pinConfig::led2, pinConfig::led3, pinConfig::led4};
    constexpr uint8_t ledCount = sizeof(pins) / sizeof(pins[0]);
    constexpr uint32_t pinActiveLevel = 1;
    constexpr uint32_t pinInactiveLevel = 0;
}

namespace digitDisplayConfig
{
    constexpr gpio_num_t segmentPins[] = {
        pinConfig::digitSegmentA, // 0b00000001
        pinConfig::digitSegmentB, // 0b00000010
        pinConfig::digitSegmentC, // 0b00000100
        pinConfig::digitSegmentD, // 0b00001000
        pinConfig::digitSegmentE, // 0b00010000
        pinConfig::digitSegmentF, // 0b00100000
        pinConfig::digitSegmentG, // 0b01000000
    };
    constexpr uint8_t segmentCount = sizeof(digitDisplayConfig::segmentPins) / sizeof(digitDisplayConfig::segmentPins[0]);
    constexpr gpio_num_t activatorPins[] = {
        pinConfig::digitActivator1,
        pinConfig::digitActivator2,
    };
    constexpr uint8_t digitCount = sizeof(digitDisplayConfig::activatorPins) / sizeof(digitDisplayConfig::activatorPins[0]);
    static_assert(digitCount <= 4, "Segments/activators count should be less or equal to 4");
    constexpr int segmentActiveLevel = 0;
    constexpr int segmentInactiveLevel = 1;
    constexpr int activatorActiveLevel = 0;
    constexpr int activatorInactiveLevel = 1;
    constexpr uint8_t digits[] = {
        0b00111111, // 0 (A, B, C, D, E, F)
        0b00000110, // 1 (B, C)
        0b01011011, // 2 (A, B, D, E, G)
        0b01001111, // 3 (A, B, C, D, G)
        0b01100110, // 4 (B, C, F, G)
        0b01101101, // 5 (A, C, D, F, G)
        0b01111101, // 6 (A, C, D, E, F, G)
        0b00000111, // 7 (A, B, C)
        0b01111111, // 8 (A, B, C, D, E, F, G)
        0b01101111, // 9 (A, B, C, D, F, G)
    };
    constexpr uint32_t timerResolutionHz = 1 * 1000 * 1000; // 1 MHz
    constexpr uint32_t scanIntervalUs = 10000;
}

namespace melodyConfig
{
    constexpr uint32_t maxNotes = helpers::pow10[digitDisplayConfig::digitCount] - 1;
    constexpr uint16_t restBetweenNotesMs = 50;
}

namespace toneConfig
{
    constexpr uint16_t C4 = 262;
    constexpr uint16_t D4 = 294;
    constexpr uint16_t E4 = 330;
    constexpr uint16_t F4 = 349;
    constexpr uint16_t G4 = 392;
    constexpr uint16_t A4 = 440;
    constexpr uint16_t B4 = 494;
    constexpr uint16_t C5 = 523;
    constexpr uint16_t rest = 0;

    constexpr uint16_t tones[] = {C4, D4, E4, F4, G4, A4, B4, C5, rest};
    constexpr int count = sizeof(tones) / sizeof(tones[0]);
}

namespace durationConfig
{
    constexpr uint16_t half = 800;
    constexpr uint16_t quarter = 400;
    constexpr uint16_t eighth = 200;

    struct DurationLedConfig
    {
        uint16_t durationMs;
        uint8_t ledCount;
    };

    constexpr DurationLedConfig durationLedConfig[] = {
        {durationConfig::half, 4},
        {durationConfig::quarter, 2},
        {durationConfig::eighth, 1},
    };

    constexpr int durationCount = sizeof(durationLedConfig) / sizeof(durationLedConfig[0]);

    constexpr bool checkLedCountNotExceedMax()
    {
        for (size_t i = 0; i < durationCount; ++i)
        {
            if (durationLedConfig[i].ledCount > ledConfig::ledCount)
                return false;
        }
        return true;
    }

    static_assert(checkLedCountNotExceedMax(), "ledCount must be less than or equal to ledConfig::ledCount");
}

namespace superloopConfig
{
    constexpr uint32_t yieldDelayMs = 10;
}
