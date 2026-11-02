#pragma once

#include <cstddef>
#include <cstdint>

#include "phase_table.h"

class TrafficFsm {
public:
    void reset();

    // Advance time; returns true if at least one phase transition occurred.
    bool advance(uint32_t deltaMs);

    uint8_t lampMask() const;      // R/Y/G bits, blink already resolved
    uint16_t remainingSeconds() const;  // countdown for the current logical group
    uint32_t remainingMs() const;

    size_t index() const { return index_; }
    const char* phaseName() const { return phases::kPhases[index_].name; }
    uint32_t phaseDurationMs() const { return phases::kPhases[index_].durationMs; }

private:
    size_t index_ = 0;
    uint32_t elapsedInPhaseMs_ = 0;
    uint32_t groupElapsedMs_ = 0;
};
