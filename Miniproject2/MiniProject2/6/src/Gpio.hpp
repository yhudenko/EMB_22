#ifndef GPIO_HPP
#define GPIO_HPP

#include <cstdint>

void gpioInit();
void turnOffDigits();
void activateDigit(uint8_t digit);
void showSegments(uint8_t mask);
uint16_t readPotValue();

#endif // GPIO_HPP