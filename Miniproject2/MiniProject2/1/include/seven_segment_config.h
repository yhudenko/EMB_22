#pragma once

#include <Arduino.h>

constexpr uint8_t kDisplayEnablePins[] = {
    15, 16
};

constexpr uint8_t kSegmentPins[8] = {
    10, // A
    11, // B
    6, // C
    5, // D
    4, // E
    9, // F
    8, // G
    7  // DP
};
constexpr uint8_t kDecimalPointPin = 7;

constexpr uint8_t undefinedDigitPattern = 0b01000000; // Only middle led

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

constexpr uint16_t displayUpdateIntervalMs = 1000;
constexpr uint16_t digitScanIntervalUs = 4000;