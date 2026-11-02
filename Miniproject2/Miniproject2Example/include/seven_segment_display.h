#pragma once

#include <Arduino.h>

class SevenSegmentDisplay {
public:
    void begin();
    void setNumber(int value);  // 0..99 shown; anything else blanks
    void scan();                // call every loop iteration

private:
    void driveDigit(uint8_t digit, int glyphValue);

    int value_ = -1;
    uint8_t activeDigit_ = 0;
    uint32_t lastScanUs_ = 0;
};
