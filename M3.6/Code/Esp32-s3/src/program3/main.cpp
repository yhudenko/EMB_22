#include <Arduino.h>

// Пасивний п'єзозумер без плати управління: GPIO7 -> резистор 100–330 Ом -> «+», «-» -> GND.
// Динамік або потужний зумер підключати лише через відповідний драйвер.
constexpr uint8_t BUZZER_PIN = 7;
constexpr uint8_t BUZZER_CHANNEL = 0;
constexpr uint8_t PWM_RESOLUTION = 10;

// Музичні ноти (частоти в Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

void playNote(int frequency, int durationMs) {
    if (frequency <= 0) {
        ledcWrite(BUZZER_CHANNEL, 0);
    } else {
        ledcWriteTone(BUZZER_CHANNEL, frequency); // duty = 50%
    }

    delay(durationMs);

    ledcWrite(BUZZER_CHANNEL, 0);

    delay(50);  // Пауза між нотами
}

void setup() {
    Serial0.begin(115200);

    ledcSetup(BUZZER_CHANNEL, NOTE_C4, PWM_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
}

void loop() {
    // Мелодія "Twinkle, Twinkle, Little Star"
    // Do Do Sol Sol La La Sol
    playNote(NOTE_C4, 400);   // Do
    playNote(NOTE_C4, 400);   // Do
    playNote(NOTE_G4, 400);   // Sol
    playNote(NOTE_G4, 400);   // Sol
    playNote(NOTE_A4, 400);   // La
    playNote(NOTE_A4, 400);   // La
    playNote(NOTE_G4, 800);   // Sol (довша нота)
    // Fa Fa Mi Mi Re Re Do
    playNote(NOTE_F4, 400);   // Fa
    playNote(NOTE_F4, 400);   // Fa
    playNote(NOTE_E4, 400);   // Mi
    playNote(NOTE_E4, 400);   // Mi
    playNote(NOTE_D4, 400);   // Re
    playNote(NOTE_D4, 400);   // Re
    playNote(NOTE_C4, 800);   // Do (довша)
    // Пауза перед повтором мелодії
    delay(2000);
}