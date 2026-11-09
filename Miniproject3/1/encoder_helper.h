#pragma once

#include <Bounce2.h>
#include <RotaryEncoder.h>

#define ENCODER_PIN_CLK 1
#define ENCODER_PIN_DT 2
#define ENCODER_PIN_SW 38

extern RotaryEncoder encoder;
extern Bounce2::Button button;
extern int previousPosition;

void setupEncoder();