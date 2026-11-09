#include "DisplayManager.hpp"
#include "Config.hpp"
#include "esp_timer.h"
#include <string.h>

static const uint8_t segMap[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90
};
constexpr uint8_t SEG_DASH = 0xBF;

DisplayManager::DisplayManager() : _mode(DisplayMode::DASHES), _activeDigitsCount(3), _currentMuxIdx(0), _lastMuxTime(0) {
    memset(_digits, 0, sizeof(_digits));
    memset(_dots, 0, sizeof(_dots));
    _dots[0] = true;
}

void DisplayManager::init() {
    gpio_set_level(Config::digit1Pin, 1);
    gpio_set_level(Config::digit2Pin, 1);
    gpio_set_level(Config::digit3Pin, 1);

    gpio_num_t segments[] = {Config::segAPin, Config::segBPin, Config::segCPin, Config::segDPin, 
                             Config::segEPin, Config::segFPin, Config::segGPin, Config::segDpPin};
    for (int i = 0; i < 8; i++) gpio_set_level(segments[i], 1);

    uint64_t mask = (1ULL << Config::segAPin) | (1ULL << Config::segBPin) | 
                    (1ULL << Config::segCPin) | (1ULL << Config::segDPin) | 
                    (1ULL << Config::segEPin) | (1ULL << Config::segFPin) | 
                    (1ULL << Config::segGPin) | (1ULL << Config::segDpPin) |
                    (1ULL << Config::digit1Pin) | (1ULL << Config::digit2Pin) | 
                    (1ULL << Config::digit3Pin);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);
}

void DisplayManager::setMode(DisplayMode mode) { _mode = mode; }

void DisplayManager::setDigits(uint8_t d1, uint8_t d2, uint8_t d3) {
    _digits[0] = d1; _digits[1] = d2; _digits[2] = d3;
}

void DisplayManager::setDigitsArray(const uint8_t* arr) {
    _digits[0] = arr[0]; _digits[1] = arr[1]; _digits[2] = arr[2];
}

void DisplayManager::setDots(bool b1, bool b2, bool b3) {
    _dots[0] = b1; _dots[1] = b2; _dots[2] = b3;
}

void DisplayManager::setActiveCount(uint8_t count) { _activeDigitsCount = count; }

void DisplayManager::tick() {
    int64_t now = esp_timer_get_time();
    
    // Перемикаємо розряд кожні 3000 мікросекунд (3 мілісекунди)
    if (now - _lastMuxTime < 3000) {
        return; 
    }
    _lastMuxTime = now;

    gpio_num_t digitsPins[] = {Config::digit1Pin, Config::digit2Pin, Config::digit3Pin};
    gpio_num_t segPins[] = {Config::segAPin, Config::segBPin, Config::segCPin, Config::segDPin, 
                            Config::segEPin, Config::segFPin, Config::segGPin, Config::segDpPin};

    //Гасимо поточний анод
    for (int d = 0; d < 3; d++) {gpio_set_level(digitsPins[d], 1);}

    if (_mode == DisplayMode::BLANK) return;

    //Визначаємо наступний розряд для підсвічування
    _currentMuxIdx++;
    if (_currentMuxIdx >= _activeDigitsCount) {
        _currentMuxIdx = 0;
    }

    //Формуємо маску сегментів
    uint8_t mask = 0xFF;
    if (_mode == DisplayMode::DASHES) {
        mask = SEG_DASH;
    } else if (_mode == DisplayMode::VALUES) {
        mask = segMap[_digits[_currentMuxIdx]];
    }

    if (_dots[_currentMuxIdx]) mask &= ~(1 << 7);

    //Записуємо маску в катоди
    for (int s = 0; s < 8; s++) {
        gpio_set_level(segPins[s], (mask >> s) & 0x01);
    }

    //Запалюємо новий анод
    gpio_set_level(digitsPins[_currentMuxIdx], 0);
}
