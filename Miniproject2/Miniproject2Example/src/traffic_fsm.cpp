#include "traffic_fsm.h"

#include "timing.h"

using phases::kPhases;
using phases::kPhaseCount;

void TrafficFsm::reset() {
    index_ = 0;
    elapsedInPhaseMs_ = 0;
    groupElapsedMs_ = 0;
}

bool TrafficFsm::advance(uint32_t deltaMs) {
    bool transitioned = false;
    elapsedInPhaseMs_ += deltaMs;
    groupElapsedMs_ += deltaMs;

    while (elapsedInPhaseMs_ >= kPhases[index_].durationMs) {
        const uint8_t prevGroup = kPhases[index_].logicalGroup;
        elapsedInPhaseMs_ -= kPhases[index_].durationMs;
        index_ = (index_ + 1) % kPhaseCount;
        transitioned = true;
        
        if (kPhases[index_].logicalGroup != prevGroup) {
            groupElapsedMs_ = elapsedInPhaseMs_;
        }
    }
    return transitioned;
}

uint8_t TrafficFsm::lampMask() const {
    const phases::PhaseInfo& p = kPhases[index_];
    if (p.blink && (elapsedInPhaseMs_ / timing::kBlinkPeriodMs) % 2 == 1) {
        return phases::kMaskNone;
    }
    return p.lampMask;
}

uint32_t TrafficFsm::remainingMs() const {
    const uint32_t groupTotal = phases::groupStartMs(index_);
    return groupTotal > groupElapsedMs_ ? groupTotal - groupElapsedMs_ : 0;
}

uint16_t TrafficFsm::remainingSeconds() const {
    const uint32_t ms = remainingMs();
    return static_cast<uint16_t>((ms + 999) / 1000);
    // return static_cast<uint16_t>(ceilf(ms * 0.001f)); need #include <math.h>
}
