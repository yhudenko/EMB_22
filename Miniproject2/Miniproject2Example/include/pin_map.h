#pragma once

#include <cstdint>

namespace pins {

// Traffic LEDs (active HIGH, cathode -> GND via ~330R)
constexpr uint8_t kRedLed = 48;
constexpr uint8_t kYellowLed = 47;
constexpr uint8_t kGreenLed = 21;

// 2-digit 7-segment, segments A..G (active LOW on common-anode).
// Index order matches the segment letter: [0]=A [1]=B [2]=C [3]=D [4]=E [5]=F [6]=G
constexpr uint8_t kSegments[7] = {
    4,   // A
    5,   // B
    6,   // C
    7,   // D
    15,  // E
    16,  // F
    17,  // G
};

// Digit common-anode enable pins (active HIGH through a driver)
constexpr uint8_t kDigitEnable[2] = {18, 8};

}  // namespace pins
