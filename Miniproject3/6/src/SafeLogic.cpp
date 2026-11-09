#include "SafeLogic.hpp"
#include "Config.hpp"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "SAFE_LOGIC";

SafeLogic::SafeLogic() : 
    _state(SafeState::IDLE),
    _btnLvl(Config::buttonLvlPin),
    _btnStart(Config::buttonStartPin),
    _encoder(Config::encoderClkPin, Config::encoderDtPin, Config::encoderSwPin),
    _currentLvl(1), _activeDigitIdx(0), _timerCountdown(0) {
    memset(_codeBuffer, 0, sizeof(_codeBuffer));
}

void SafeLogic::init() {
    _display.init();
    _servo.lock();
    resetGame();
}

uint32_t SafeLogic::calculateFNV1aHash() {
    uint32_t hash = 0x811c9dc5;
    for (int i = 0; i < _currentLvl; i++) {
        hash ^= _codeBuffer[i];
        hash *= 0x01000193;
    }
    return hash;
}

void SafeLogic::resetGame() {
    _state = SafeState::IDLE;
    memset(_codeBuffer, 0, sizeof(_codeBuffer));
    _activeDigitIdx = 0;
    _servo.lock();
    _display.setMode(DisplayMode::DASHES);
    _display.setActiveCount(3);
    
    // Відображаємо крапку відповідно до вибраного рівня
    _display.setDots(_currentLvl == 1, _currentLvl == 2, _currentLvl == 3);
}

void SafeLogic::update() {
     _display.tick();
    // Загальна кнопка скидання на Енкодері (SW)
    if (_encoder.isButtonPressed()) {
        _buzzer.playTone(800, 100);
        resetGame();
        return;
    }

    switch (_state) {
        case SafeState::IDLE: {
            if (_btnLvl.isPressed()) {
                _currentLvl++;
                if (_currentLvl > 3) _currentLvl = 1;
                _buzzer.playTone(600, 80);
                _display.setDots(_currentLvl == 1, _currentLvl == 2, _currentLvl == 3);
                ESP_LOGI(TAG, "Складність змінена на рівень: %d", _currentLvl);
            }

            if (_btnStart.isPressed()) {
                _buzzer.playTone(1000, 100);
                _state = SafeState::SETUP_TIME;
            }
            break;
        }

        case SafeState::SETUP_TIME: {
            // Опитуємо потенціометр не частіше ніж кожні 150 мілісекунд
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - _lastTimerTick >= 150) {
                _lastTimerTick = now_ms;
                
                uint32_t newTime = _pot.getTimeLimitSeconds();
                
                // Додаємо фільтр шуму: оновлюємо екран тільки якщо ручку реально зрушили
                // (абсолютна різниця між значеннями більша за 1 секунду)
                if (abs((int)newTime - (int)_timerCountdown) > 1 || _timerCountdown == 0) {
                    _timerCountdown = newTime;
                    
                    uint8_t d1 = _timerCountdown / 100;
                    uint8_t d2 = (_timerCountdown % 100) / 10;
                    uint8_t d3 = _timerCountdown % 10;
                    
                    _display.setDigits(d1, d2, d3);
                    _display.setMode(DisplayMode::VALUES);
                    _display.setDots(false, false, false);
                }
            }

            if (_btnStart.isPressed()) {
                _buzzer.playTone(1200, 200);
                _state = SafeState::INPUT_CODE;
                _display.setActiveCount(_currentLvl);
                
                memset(_codeBuffer, 0, sizeof(_codeBuffer));
                _display.setDigitsArray(_codeBuffer);
                
                _activeDigitIdx = 0;
                _display.setDots(true, false, false); 
                _lastTimerTick = esp_timer_get_time() / 1000; // Скидаємо таймер під відлік
            }
            break;
        }


        case SafeState::INPUT_CODE: {
            // Обробка таймера гри
            int64_t now_ms = esp_timer_get_time() / 1000; 
            if (now_ms - _lastTimerTick >= 1000) {
                _lastTimerTick = now_ms;
                if (_timerCountdown > 0) {
                    _timerCountdown--;
                    if (_timerCountdown <= 3) _buzzer.playTone(400, 50); 
                } else {
                    _state = SafeState::FAIL;
                    _buzzer.playFail();
                    _display.setMode(DisplayMode::BLANK);
                    break;
                }
            }

            // Логіка обертання енкодера
            int rot = _encoder.updateRotation();
            if (rot == 1 || rot == -1) { 
                if (rot == 1) {
                    _codeBuffer[_activeDigitIdx] = (_codeBuffer[_activeDigitIdx] + 1) % 10;
                } else {
                    _codeBuffer[_activeDigitIdx] = (_codeBuffer[_activeDigitIdx] == 0) ? 9 : _codeBuffer[_activeDigitIdx] - 1;
                }
                _buzzer.playTone(900, 30);

                _display.setDigitsArray(_codeBuffer); 
            } else if (rot == 2) { 
                _buzzer.playTone(1500, 100);
                _activeDigitIdx++;
                if (_activeDigitIdx >= _currentLvl) _activeDigitIdx = 0;
                _display.setDots(_activeDigitIdx == 0, _activeDigitIdx == 1, _activeDigitIdx == 2);
            }

            // Натискання кнопки для фінальної перевірки
            if (_btnStart.isPressed()) {
                uint32_t currentHash = calculateFNV1aHash();
                uint32_t targetHash = 0;

                if (_currentLvl == 1) targetHash = Config::HASH_LVL1;
                else if (_currentLvl == 2) targetHash = Config::HASH_LVL2;
                else if (_currentLvl == 3) targetHash = Config::HASH_LVL3;

                ESP_LOGI(TAG, "Введений Хеш: 0x%08lx | Цільовий Хеш: 0x%08lx", currentHash, targetHash);

                if (currentHash == targetHash) {
                    _state = SafeState::SUCCESS;
                    _servo.unlock();
                    _buzzer.playWin();
                } else {
                    _state = SafeState::FAIL;
                    _buzzer.playFail();
                    _display.setMode(DisplayMode::BLANK);
                }
            }
            break;
        }

        case SafeState::SUCCESS:
        case SafeState::FAIL:
            // Чекаємо кнопки Reset на енкодері для виходу
            break;
    }
}
