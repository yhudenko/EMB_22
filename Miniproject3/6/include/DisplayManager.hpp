#pragma once
#include <stdint.h>

enum class DisplayMode {
    DASHES,
    VALUES,
    BLANK
};

class DisplayManager {
private:
    uint8_t _digits[3];
    bool _dots[3];
    DisplayMode _mode;
    uint8_t _activeDigitsCount;
    uint8_t _currentMuxIdx; // Який розряд світиться прямо зараз
    int64_t _lastMuxTime;   // Коли останній раз перемикали розряд

public:
    DisplayManager();
    void init();
    void tick();
    
    void setMode(DisplayMode mode);
    void setDigits(uint8_t d1, uint8_t d2, uint8_t d3);
    void setDigitsArray(const uint8_t* arr);
    void setDots(bool b1, bool b2, bool b3);
    void setActiveCount(uint8_t count);
};
