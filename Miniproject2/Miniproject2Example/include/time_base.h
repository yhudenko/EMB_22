#pragma once

#include <Arduino.h>

namespace TimeBase {

void begin();

// Milliseconds elapsed since begin() consumed by the caller. Returns the delta
// since the previous call and resets the internal accumulator.
uint32_t takeElapsedMs();

}  // namespace TimeBase
