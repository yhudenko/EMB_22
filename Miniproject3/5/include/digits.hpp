#pragma once

#include <stdint.h>

namespace digits
{
    void init();
    void activateDigit(uint8_t digit);
    void showSegments(uint8_t mask);
    bool consumeSwitchRequest();
    void turnOff();

}