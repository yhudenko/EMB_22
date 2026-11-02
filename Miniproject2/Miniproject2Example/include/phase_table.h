#pragma once

#include <cstddef>
#include <cstdint>

#include "timing.h"

namespace phases {

// Lamp bits (R/Y/G).
constexpr uint8_t kMaskNone = 0b000;
constexpr uint8_t kMaskGreen = 0b001;
constexpr uint8_t kMaskYellow = 0b010;
constexpr uint8_t kMaskRed = 0b100;

struct PhaseInfo {
    const char* name;
    uint8_t lampMask;
    uint32_t durationMs;
    uint8_t logicalGroup;  // phases sharing an id share one continuous countdown
    bool blink;            // apply blink effect to the lamp while active
};

constexpr PhaseInfo kPhases[] = {
    {"GREEN", kMaskGreen, timing::kGreenMs, 0, false},
    {"GREEN_BLINK", kMaskGreen, timing::kGreenBlinkMs, 0, true},
    {"YELLOW", kMaskYellow, timing::kYellowMs, 1, false},
    {"RED", kMaskRed, timing::kRedMs, 2, false},
    {"RED_YELLOW", kMaskRed | kMaskYellow, timing::kRedYellowMs, 2, false},
};

constexpr size_t kPhaseCount = sizeof(kPhases) / sizeof(kPhases[0]);

// Total duration of a logical group starting at `index` (sums consecutive
// phases that share the same group id), used for the shared countdown.
inline uint32_t groupStartMs(size_t index) {
    uint32_t total = 0;
    size_t i = index;
    // Walk back to the first phase of this group.
    while (i > 0 && kPhases[i - 1].logicalGroup == kPhases[index].logicalGroup) {
        --i;
    }
    while (i < kPhaseCount && kPhases[i].logicalGroup == kPhases[index].logicalGroup) {
        total += kPhases[i].durationMs;
        ++i;
    }
    return total;
}

}  // namespace phases
