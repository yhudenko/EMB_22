#pragma once

#include <cstdint>

namespace buzzer
{
    void init();
    void playNote(uint16_t frequency);
    void stopNote();
} // namespace buzzer