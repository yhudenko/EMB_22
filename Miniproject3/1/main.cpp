#include "RotaryEncoder.h"

#include "buzzer_helper.h"
#include "encoder_helper.h"
#include "display_helper.h"

static uint8_t getNewRandomNumber(uint8_t oldNumber) {
    uint8_t n;
    do { n = random(0, 10); } while (n == oldNumber);
    return n;
}

static uint8_t valueToGuess = 0;
static uint8_t currentAttempt = 0;


void setup() {
    Serial.begin(115200);
    setupBuzzer();
    setupEncoder();
    setupDisplay();

    randomSeed(analogRead(13));

    valueToGuess = getNewRandomNumber(valueToGuess);
}

void loop() {
    encoder.tick();
    button.update();


    int currentPosition = encoder.getPosition();
    if (currentPosition != previousPosition) {
        if (currentPosition > 9) {
            currentPosition = 0;
            encoder.setPosition(0);
        } else if (currentPosition < 0) {
            currentPosition = 9;
            encoder.setPosition(9);
        }

        previousPosition = currentPosition;
        showDigit(currentPosition);

        if (currentPosition == valueToGuess) {
            playMelody(PING_MELODY);
        }
    }

    if (button.pressed()) {
        bool toResetAttempt = false;
        if (currentPosition == valueToGuess) {
            digitalWrite(ATTEMPT_LEDS[currentAttempt], HIGH);
            if (currentAttempt == 3) {
                toResetAttempt = true;
                resetDisplay();
                playMelody(SUCCESS_MELODY);
            } else {
                currentAttempt++;
            }

        } else {
            toResetAttempt = true;
            resetDisplay();
            playMelody(FAIL_MELODY);
        }

        if (toResetAttempt == true) {
            resetLeds();
            currentAttempt = 0;
            encoder.setPosition(0);
            previousPosition = 0;
            showDigit(0);
        }

        valueToGuess = getNewRandomNumber(previousPosition);
    }
}