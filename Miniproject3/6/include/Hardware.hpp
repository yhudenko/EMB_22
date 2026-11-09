#pragma once
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

class Button {
private:
    gpio_num_t _pin;
    bool _lastState;
public:
    Button(gpio_num_t pin);
    bool isPressed(); 
};

class Buzzer {
public:
    Buzzer();
    void playTone(uint32_t freq, uint32_t duration_ms);
    void playWin();
    void playFail();
};

class ServoLock {
public:
    ServoLock();
    void lock();
    void unlock();
};

class Potentiometer {
private:
    adc_oneshot_unit_handle_t _adc_handle;
public:
    Potentiometer();
    uint32_t getTimeLimitSeconds(); // АЦП у діапазон 10-60 сек
};
