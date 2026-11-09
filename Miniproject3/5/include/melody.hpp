#pragma once

#include "config.hpp"
#include <cstdint>

struct Note
{
    uint16_t toneHz;
    uint16_t durationMs;
};

class Melody
{
public:
    uint32_t getNoteCount() const;
    void addNote(uint16_t toneHz, uint16_t durationMs);
    uint32_t playMelody(uint32_t currentTimeMs);
    void stopMelody();
    void clearMelody();

private:
    uint32_t getNotesLeftToPlay() const;

    Note notes_[melodyConfig::maxNotes];
    uint32_t noteCount_ = 0;
    uint32_t lastNotePlayedMs_ = 0;
    uint32_t currentNoteIndex_ = 0;
    bool isPlaying_ = false;
};