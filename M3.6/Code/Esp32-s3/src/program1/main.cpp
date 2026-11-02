#include <Arduino.h>
#include <Bounce2.h>
#include <RotaryEncoder.h>

// KY-040: + -> 3V3, GND -> GND.
// CLK -> GPIO4, DT -> GPIO5, SW -> GPIO6
// Для правильного напрямку CLK -> GPIO5, DT -> GPIO4, SW -> GPIO6
constexpr uint8_t PIN_CLK = 5;
constexpr uint8_t PIN_DT = 4;
constexpr uint8_t PIN_SW = 6;

// FOUR3: один крок після повного квадратурного циклу,
// коли обидва контакти знову HIGH. Конструктор вмикає INPUT_PULLUP.
// FOUR0 при LOW
// TWO03 потрібно для енодерів які клацать 2 рази за квадратурний цикл
RotaryEncoder encoder(PIN_CLK, PIN_DT, RotaryEncoder::LatchMode::FOUR3);
Bounce2::Button button;

long previousPosition = 0;

void setup()
{
    Serial0.begin(115200);

    button.attach(PIN_SW, INPUT_PULLUP);
    button.interval(20);
    button.setPressedState(LOW);

    Serial0.println("Position = 0");
}

void loop()
{
    encoder.tick();
    button.update();

    const long position = encoder.getPosition();
    if (position != previousPosition)
    {
        Serial0.printf("%s | Position = %ld\n",
                      position > previousPosition ? "CW (+)" : "CCW (-)",
                      position);
        previousPosition = position;
    }

    if (button.pressed())
    {
        encoder.setPosition(0);
        previousPosition = 0;
        Serial0.println("Button pressed: Position = 0");
    }
}