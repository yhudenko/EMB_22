#pragma once
#include "driver/gpio.h"

class Encoder {
private:
    gpio_num_t _clk;
    gpio_num_t _dt;
    gpio_num_t _sw;
    
    int _lastClkState;
    int _pulseCount;
    int _lastDirection; // 1 - за годинниковою, -1 - проти, 0 - стоїть
    bool _btnLastState;

public:
    Encoder(gpio_num_t clk, gpio_num_t dt, gpio_num_t sw);
    
    int updateRotation();
    
    bool isButtonPressed();
};
