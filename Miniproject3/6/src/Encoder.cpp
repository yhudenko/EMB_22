#include "Encoder.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

Encoder::Encoder(gpio_num_t clk, gpio_num_t dt, gpio_num_t sw) 
    : _clk(clk), _dt(dt), _sw(sw), _pulseCount(0), _lastDirection(0), _btnLastState(true) {
    
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << clk) | (1ULL << dt) | (1ULL << sw);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    _lastClkState = gpio_get_level(_clk);
}

int Encoder::updateRotation() {
    int currentClk = gpio_get_level(_clk);
    int result = 0;

    if (currentClk != _lastClkState && currentClk == 0) { // Спад сигналу CLK
        int dir = (gpio_get_level(_dt) != currentClk) ? 1 : -1;
        
        // Перевіряємо реверс: якщо ми крутили в один бік, а тепер крутимо в інший
        if (_lastDirection != 0 && dir != _lastDirection) {
            _lastDirection = dir;
            _pulseCount = 0;
            _lastClkState = currentClk;
            return 2; // Зміна напрямку. Сигнал підтвердження цифри сейфу
        }
        
        _lastDirection = dir;
        _pulseCount += dir;

        // поріг 2 для швидшого перемикання цифр
        if (_pulseCount >= 2) {
            _pulseCount = 0;
            result = 1;
        } else if (_pulseCount <= -2) {
            _pulseCount = 0;
            result = -1;
        }
    }
    _lastClkState = currentClk;
    return result;
}


bool Encoder::isButtonPressed() {
    bool currentSw = gpio_get_level(_sw);
    if (_btnLastState && !currentSw) {
        vTaskDelay(pdMS_TO_TICKS(50));
        _btnLastState = currentSw;
        return true;
    }
    _btnLastState = currentSw;
    return false;
}
