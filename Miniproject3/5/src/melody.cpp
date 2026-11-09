#include "melody.hpp"
#include "buzzer.hpp"

uint32_t Melody::getNotesLeftToPlay() const
{
    return noteCount_ - currentNoteIndex_; // Return the number of notes left to play
}

uint32_t Melody::getNoteCount() const
{
    return noteCount_;
}

void Melody::addNote(uint16_t toneHz, uint16_t durationMs)
{
    if (noteCount_ < melodyConfig::maxNotes)
    {
        notes_[noteCount_++] = {toneHz, durationMs};
    }
}

uint32_t Melody::playMelody(uint32_t currentTimeMs)
{
    if (currentNoteIndex_ >= noteCount_)
    {
        buzzer::stopNote();
        isPlaying_ = false;
        return 0;
    }

    uint32_t elapsedTime = currentTimeMs - lastNotePlayedMs_;

    uint16_t frequency = notes_[currentNoteIndex_].toneHz;
    uint16_t durationMs = notes_[currentNoteIndex_].durationMs;

    // The first note played
    if (lastNotePlayedMs_ == 0)
    {
        lastNotePlayedMs_ = currentTimeMs;
        elapsedTime = 0;
    }

    // Check if the current note has finished playing
    if (elapsedTime >= durationMs)
    {
        currentNoteIndex_++;
        if (currentNoteIndex_ >= noteCount_)
        {
            buzzer::stopNote();
            isPlaying_ = false;
            return 0;
        }

        lastNotePlayedMs_ = currentTimeMs;
        elapsedTime = currentTimeMs - lastNotePlayedMs_;
        frequency = notes_[currentNoteIndex_].toneHz;
        durationMs = notes_[currentNoteIndex_].durationMs;
    }

    const bool isPauseBetweenNotes = elapsedTime > (durationMs - melodyConfig::restBetweenNotesMs);
    if (isPauseBetweenNotes)
    {
        buzzer::stopNote();
        isPlaying_ = false;
        return getNotesLeftToPlay();
    }

    if (isPlaying_)
    {
        return getNotesLeftToPlay();
    }

    if (frequency == toneConfig::rest)
    {
        buzzer::stopNote();
    }
    else
    {
        buzzer::playNote(frequency);
    }
    isPlaying_ = true;

    return getNotesLeftToPlay();
}

void Melody::stopMelody()
{
    buzzer::stopNote();
    isPlaying_ = false;
    currentNoteIndex_ = 0;
    lastNotePlayedMs_ = 0;
}

void Melody::clearMelody()
{
    stopMelody();
    noteCount_ = 0;
}