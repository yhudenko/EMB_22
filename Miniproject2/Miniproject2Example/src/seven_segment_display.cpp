#include "seven_segment_display.h"

#include "pin_map.h"
#include "timing.h"

namespace {

constexpr uint8_t kGlyphs[10] = {
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

constexpr uint8_t kDashPattern = 0b1000000;
constexpr uint8_t kBlankPattern = 0b0000000;

constexpr uint8_t kDigitCount = 2;
constexpr uint8_t kSegmentCount = 7;

// Special glyph codes passed to driveDigit() (non-digit).
constexpr int kBlankGlyph = -1;
constexpr int kDashGlyph = -2;

// Segments are active LOW (common-anode): drive LOW to light, HIGH to turn off.
constexpr uint8_t kSegmentOn = LOW;
constexpr uint8_t kSegmentOff = HIGH;

// Digit enable is active HIGH (through a driver): HIGH selects a digit, LOW off.
constexpr uint8_t kDigitOn = HIGH;
constexpr uint8_t kDigitOff = LOW;

}  // namespace

void SevenSegmentDisplay::begin() {
    for (uint8_t i = 0; i < kSegmentCount; ++i) {
        pinMode(pins::kSegments[i], OUTPUT);
        digitalWrite(pins::kSegments[i], kSegmentOff);
    }
    for (uint8_t d = 0; d < kDigitCount; ++d) {
        pinMode(pins::kDigitEnable[d], OUTPUT);
        digitalWrite(pins::kDigitEnable[d], kDigitOff);
    }
}

void SevenSegmentDisplay::setNumber(int value) {
    value_ = value;
}

void SevenSegmentDisplay::driveDigit(uint8_t digit, int glyphValue) {
    // Disable both digits
    for (uint8_t d = 0; d < kDigitCount; ++d) {
        digitalWrite(pins::kDigitEnable[d], kDigitOff);
    }

    const uint8_t pattern = (glyphValue >= 0 && glyphValue <= 9) ? kGlyphs[glyphValue]
                            : (glyphValue == kDashGlyph)         ? kDashPattern
                                                                 : kBlankPattern;
    for (uint8_t i = 0; i < kSegmentCount; ++i) {
        digitalWrite(pins::kSegments[i], (pattern & (1 << i)) ? kSegmentOn : kSegmentOff);
    }

    digitalWrite(pins::kDigitEnable[digit], kDigitOn);
}

void SevenSegmentDisplay::scan() {
    const uint32_t nowUs = micros();
    if (nowUs - lastScanUs_ < timing::kDigitScanIntervalUs) {
        return;
    }
    lastScanUs_ = nowUs;

    int glyph = kDashGlyph;  // out-of-range shows "--"
    if (value_ >= 0 && value_ <= 99) {
        const int tens = value_ / 10; // possible use *0.1f, but for int result is slower
        const int ones = value_ % 10;
        if (activeDigit_ == 0) {
            glyph = (tens == 0) ? kBlankGlyph : tens;  // suppress leading zero
        } else {
            glyph = ones;
        }
    }

    driveDigit(activeDigit_, glyph);
    activeDigit_ = (activeDigit_ + 1) % kDigitCount;
}
