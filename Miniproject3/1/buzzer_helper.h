#pragma once

#include "Arduino.h"
#include <driver/ledc.h>

#define BUZZER_PIN 47
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define PWM_RESOLUTION 10

#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

struct Note {
    uint16_t frequency;
    uint16_t duration;
};

constexpr Note FAIL_MELODY[] = {
    Note{NOTE_A4, 1000},
    Note{NOTE_D4, 1000},
    Note{NOTE_C4, 500},
};
constexpr Note SUCCESS_MELODY[] = {
    {NOTE_C4, 150},
    {NOTE_C5, 150},
    {NOTE_G4, 150},
    {0, 200},
    {NOTE_F4, 150},
    {NOTE_E4, 150},
    {NOTE_D4, 150},

    {NOTE_C4, 150},
    {NOTE_D4, 150},
    {NOTE_E4, 150},
};
constexpr Note PING_MELODY[] = {
    Note{NOTE_C4, 20},
};

void playNote(Note note);

void setupBuzzer();

template<size_t N>
void playMelody(const Note (&melody)[N]) {
    for (const Note note: melody) {
        playNote(note);
    }
}
