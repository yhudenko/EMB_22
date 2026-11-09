#include "config.hpp"
#include "buttons.hpp"
#include "servo.hpp"
#include "buzzer.hpp"
#include "encoder.hpp"
#include "melody.hpp"
#include "leds.hpp"
#include "digits.hpp"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

namespace
{
    const char *TAG = "MAIN";
    int currentNoteIndex = -1; // -1 indicates no note is currently selected
    uint16_t currentDurationIndex = 0;
    uint32_t lastNotePlayedMs = 0;

    uint16_t getAngleForNoteIndex(uint16_t noteIndex)
    {
        return noteIndex * servoConfig::maxAngle / (toneConfig::count - 1);
    }

    enum class State
    {
        SelectNote,
        PreviewPlayNote,
        SaveNoteToMelody,
        PlayMelody,
        Idle,
    };

    State currentState = State::SelectNote;
    Melody currentMelody;

    uint8_t activeDigit = 0;
    uint8_t digitPattern[digitDisplayConfig::digitCount] = {};

    void setDigitsPattern(uint32_t number)
    {
        uint32_t rest = number;
        for (size_t i = digitDisplayConfig::digitCount; i > 0; --i)
        {
            const uint32_t divider = helpers::pow10[i - 1];
            const uint8_t digit = rest / divider;
            rest %= divider;
            digitPattern[digitDisplayConfig::digitCount - i] = digitDisplayConfig::digits[digit];
        }
    }

    void handleButtonPress()
    {
        if (buttons::consumePlayMelodyRequest())
        {
            if (currentState == State::PlayMelody)
            {
                currentState = State::Idle;
                currentMelody.stopMelody();
                setDigitsPattern(currentMelody.getNoteCount());
            }
            else if (currentMelody.getNoteCount() > 0)
            {
                currentState = State::PlayMelody;
            }
            return;
        }

        if (buttons::consumeResetMelodyRequest())
        {
            currentMelody.clearMelody();
            currentState = State::SelectNote;
            currentNoteIndex = -1;
            currentDurationIndex = 0;
            leds::showDuration(currentDurationIndex);
            setDigitsPattern(0);
            encoder::reset();
            return;
        }

        if (buttons::consumeChangeDurationRequest())
        {
            if (currentState != State::SelectNote && currentState != State::PreviewPlayNote)
            {
                return;
            }
            currentDurationIndex = (currentDurationIndex + 1) % durationConfig::durationCount;
            leds::showDuration(currentDurationIndex);
            ESP_LOGI(TAG, "New duration index: %d", currentDurationIndex);
        }

        if (encoder::consumeAddNoteRequest())
        {
            if (currentState != State::SelectNote && currentState != State::PreviewPlayNote)
            {
                return;
            }
            currentState = State::SaveNoteToMelody;
        }
    }

    void handleNoteChange(uint32_t currentTimeMs)
    {
        int count = encoder::getCount();

        const int newNoteIndex = ((count % toneConfig::count) + toneConfig::count) % toneConfig::count;

        if (newNoteIndex != currentNoteIndex)
        {
            ESP_LOGI(TAG, "newNoteIndex: %d", newNoteIndex);
            currentNoteIndex = newNoteIndex;
            uint16_t frequency = toneConfig::tones[currentNoteIndex];
            uint16_t angle = getAngleForNoteIndex(currentNoteIndex);
            servo::setAngle(angle);
            if (frequency == toneConfig::rest)
            {
                buzzer::stopNote();
            }
            else
            {
                buzzer::playNote(frequency);
            }

            lastNotePlayedMs = currentTimeMs;
            currentState = State::PreviewPlayNote;
        }
    }

    void handleDigitsUpdate()
    {
        if (digits::consumeSwitchRequest())
        {
            digits::turnOff();
            digits::showSegments(digitPattern[activeDigit]);
            digits::activateDigit(activeDigit);
            activeDigit = (activeDigit + 1) % digitDisplayConfig::digitCount;
        }
    }

    void setup()
    {
        buttons::init();
        leds::init();
        encoder::init();
        servo::init();
        buzzer::init();
        digits::init();
        setDigitsPattern(0);
        leds::showDuration(currentDurationIndex);
    }

    void loop()
    {
        while (true)
        {
            handleButtonPress();
            const uint32_t currentTimeMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
            switch (currentState)
            {
            case State::SelectNote:
            {
                handleNoteChange(currentTimeMs);
            }
            break;
            case State::PreviewPlayNote:
            {
                handleNoteChange(currentTimeMs);
                if (currentTimeMs - lastNotePlayedMs >= durationConfig::durationLedConfig[currentDurationIndex].durationMs)
                {
                    buzzer::stopNote();
                    currentState = State::SelectNote;
                }
            }
            break;
            case State::SaveNoteToMelody:
            {
                buzzer::stopNote();
                currentMelody.addNote(toneConfig::tones[currentNoteIndex], durationConfig::durationLedConfig[currentDurationIndex].durationMs);
                setDigitsPattern(currentMelody.getNoteCount());
                currentNoteIndex = -1;
                currentDurationIndex = 0;
                leds::showDuration(currentDurationIndex);
                currentState = State::SelectNote;
                encoder::reset();
            }
            break;
            case State::PlayMelody:
            {
                const uint32_t notesLeft = currentMelody.playMelody(currentTimeMs);

                if (notesLeft == 0)
                {
                    currentState = State::Idle;
                    currentMelody.stopMelody();
                    setDigitsPattern(currentMelody.getNoteCount());
                }
                else
                {
                    setDigitsPattern(notesLeft);
                }
            }
            break;
            case State::Idle:
                break;
            }

            handleDigitsUpdate();

            vTaskDelay(pdMS_TO_TICKS(superloopConfig::yieldDelayMs));
        }
    }
} // namespace

extern "C" void app_main()
{
    setup();
    loop();
}
