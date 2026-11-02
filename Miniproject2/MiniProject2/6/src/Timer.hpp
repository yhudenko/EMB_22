#ifndef TIMER_HPP
#define TIMER_HPP

#include <cstdint>

void timerInit();
bool consumeDigitSwitchRequest();
bool consumeReadPotRequest();
uint8_t consumeAnimationStepRequest();
uint16_t getTrafficLightHalfPeriodCounter();
void startNoValueAnimationTimer();
void stopNoValueAnimationTimer();
void updateTrafficLightTimerSpeed(uint8_t booster);

#endif