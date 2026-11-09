#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Hardware.hpp"
#include "Encoder.hpp"
#include "DisplayManager.hpp"

enum class SafeState {
    IDLE,          // Очікування, світимо тире
    SETUP_TIME,    // Налаштування таймера потенціометром перед кодом
    INPUT_CODE,    // Введення пароля енкодером
    SUCCESS,       // Перемога, серво відкрита
    FAIL           // Сейф заблоковано через помилку або таймер
};

class SafeLogic {
private:
    SafeState _state;
    Button _btnLvl;
    Button _btnStart;
    Encoder _encoder;
    Buzzer _buzzer;
    ServoLock _servo;
    Potentiometer _pot;
    DisplayManager _display;

    uint8_t _currentLvl;     // Кількість цифр в коді
    uint8_t _codeBuffer[3];
    uint8_t _activeDigitIdx; // Яку цифру зараз вводимо
    
    uint32_t _timerCountdown;
    int64_t  _lastTimerTick;

    uint32_t calculateFNV1aHash(); // Функція хешування коду
    void resetGame();
public:
    SafeLogic();
    void init();
    void update();
};
