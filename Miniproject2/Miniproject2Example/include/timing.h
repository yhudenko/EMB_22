#pragma once

#include <cstdint>

namespace timing {

constexpr uint32_t kGreenMs = 9000;
constexpr uint32_t kGreenBlinkMs = 3000;
constexpr uint32_t kYellowMs = 3000;
constexpr uint32_t kRedMs = 9000;
constexpr uint32_t kRedYellowMs = 2000;

// Blink: 500 ms on / 500 ms off during GREEN_BLINK.
constexpr uint32_t kBlinkPeriodMs = 500;

// Display multiplex cadence (per digit).
constexpr uint32_t kDigitScanIntervalUs = 4000;

}  // namespace timing
