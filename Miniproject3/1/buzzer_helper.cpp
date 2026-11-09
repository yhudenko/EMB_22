#include "buzzer_helper.h"

void playNote(Note note) {
    if (note.frequency == 0) noTone(BUZZER_PIN);
    else tone(BUZZER_PIN, note.frequency, note.duration);
    delay(note.duration);
}

void setupBuzzer() {
    ledcSetup(BUZZER_CHANNEL, NOTE_C4, PWM_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
}
