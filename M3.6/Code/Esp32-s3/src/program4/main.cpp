#include <Arduino.h>

// Пасивний п'єзозумер: GPIO7 -> резистор 100–330 Ом -> «+», «-» -> GND.
// Динамік або потужний зумер підключати лише через відповідний драйвер.
constexpr uint8_t BUZZER_PIN = 7;
constexpr uint8_t BUZZER_CHANNEL = 0;
constexpr uint8_t PWM_RESOLUTION = 10;

constexpr uint16_t NOTE_C4 = 262;
constexpr uint16_t NOTE_D4 = 294;
constexpr uint16_t NOTE_E4 = 330;
constexpr uint16_t NOTE_F4 = 349;
constexpr uint16_t NOTE_G4 = 392;
constexpr uint16_t NOTE_A4 = 440;

struct Note {
    uint16_t freq;
    uint16_t duration;
};

// Twinkle, Twinkle, Little Star
const Note melody[] = {
    {NOTE_C4, 400}, {NOTE_C4, 400},
    {NOTE_G4, 400}, {NOTE_G4, 400},
    {NOTE_A4, 400}, {NOTE_A4, 400},
    {NOTE_G4, 800},
    {NOTE_F4, 400}, {NOTE_F4, 400},
    {NOTE_E4, 400}, {NOTE_E4, 400},
    {NOTE_D4, 400}, {NOTE_D4, 400},
    {NOTE_C4, 800}
};

constexpr size_t NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);
constexpr unsigned long NOTE_GAP_MS = 50;
constexpr unsigned long REPEAT_PAUSE_MS = 2000;

enum class State {
    StartNote,
    Playing,
    Gap,
    RepeatPause
};

State state = State::StartNote;
size_t noteIndex = 0;
unsigned long stateStarted = 0;

void updateMelody() {
    const unsigned long now = millis();
    const unsigned long elapsed = now - stateStarted;

    switch (state) {
        case State::StartNote:
            if (melody[noteIndex].freq > 0) {
                ledcWriteTone(BUZZER_CHANNEL, melody[noteIndex].freq);
            } else {
                ledcWrite(BUZZER_CHANNEL, 0);
            }
            stateStarted = now;
            state = State::Playing;
            break;

        case State::Playing:
            if (elapsed >= melody[noteIndex].duration) {
                ledcWrite(BUZZER_CHANNEL, 0);
                stateStarted = now;
                state = State::Gap;
            }
            break;

        case State::Gap:
            if (elapsed >= NOTE_GAP_MS) {
                ++noteIndex;
                if (noteIndex < NOTE_COUNT) {
                    state = State::StartNote;
                } else {
                    stateStarted = now;
                    state = State::RepeatPause;
                }
            }
            break;

        case State::RepeatPause:
            if (elapsed >= REPEAT_PAUSE_MS) {
                noteIndex = 0;
                state = State::StartNote;
            }
            break;
    }
}

void setup() {
    Serial0.begin(115200);

    ledcSetup(BUZZER_CHANNEL, NOTE_C4, PWM_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
}

void loop() {
    updateMelody();

    // Тут можна опитувати кнопки, датчики, керувати LED тощо.
    // Інші дії також мають бути короткими та неблокувальними.
}