#include "encoder_helper.h"

RotaryEncoder encoder(ENCODER_PIN_CLK, ENCODER_PIN_DT, RotaryEncoder::LatchMode::FOUR3);
Bounce2::Button button;
int previousPosition = 0;

void setupEncoder() {
    button.attach(ENCODER_PIN_SW, INPUT_PULLUP);
    button.interval(20);
    button.setPressedState(LOW);
}