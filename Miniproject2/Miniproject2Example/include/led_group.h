#pragma once

#include <Arduino.h>

#include "phase_table.h"

// Drives the three traffic LEDs from a 3-bit R/Y/G mask (active HIGH).
class LedGroup {
public:
    void begin();
    void write(uint8_t mask);  // bits: phases::kMaskRed/Yellow/Green

private:
    uint8_t lastMask_ = 0xFF;
};
