#pragma once

#include <Arduino.h>

#define DISPLAY_PIN 12

constexpr int ATTEMPT_LEDS[] = {15, 16, 17, 18};

constexpr uint8_t kSegmentPins[7] = {
    6, // A
    7, // B
    10, // C
    9, // D
    8, // E
    5, // F
    4, // G
};

constexpr uint8_t kDigitPatterns[10] = {
    0b00111111, // 0: A B C D E F
    0b00000110, // 1: B C
    0b01011011, // 2: A B D E G
    0b01001111, // 3: A B C D G
    0b01100110, // 4: B C F G
    0b01101101, // 5: A C D F G
    0b01111101, // 6: A C D E F G
    0b00000111, // 7: A B C
    0b01111111, // 8: A B C D E F G
    0b01101111, // 9: A B C D F G
};

void setupDisplay();
void resetLeds();
void resetDisplay();
void digitalWriteReverse(uint8_t pin, bool val);
void writeSegments(uint8_t digitsPattern);
void showDigit(uint8_t digit);