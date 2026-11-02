#include <Arduino.h>
#include <Bounce2.h>
#include <driver/pcnt.h>
#include <esp_err.h>

// KY-040: + -> 3V3, GND -> GND.
// CLK -> GPIO4, DT -> GPIO5, SW -> GPIO6.
constexpr uint8_t PIN_CLK = 4;
constexpr uint8_t PIN_DT = 5;
constexpr uint8_t PIN_SW = 6;

constexpr pcnt_unit_t ENCODER_UNIT = PCNT_UNIT_0;
constexpr int16_t COUNTER_LIMIT = 30000;
constexpr uint16_t FILTER_CYCLES = 1023;
constexpr int COUNTS_PER_STEP = 4;
constexpr uint32_t PRINT_INTERVAL_MS = 50;

Bounce2::Button button;
int16_t previousCount = 0;
uint32_t lastPrintTime = 0;

void setupEncoder()
{
    // Два канали одного PCNT-блоку змінюють спільний лічильник.
    // Канал 0: фронти CLK, напрямок залежить від рівня DT.
    pcnt_config_t config = {};
    // GPIO, на якому рахуються фронти сигналу.
    config.pulse_gpio_num = PIN_CLK;
    // GPIO, рівень якого керує напрямком підрахунку.
    config.ctrl_gpio_num = PIN_DT;
    // Дія при LOW на керувальному GPIO.
    config.lctrl_mode = PCNT_MODE_REVERSE;
    // Дія при HIGH на керувальному GPIO.
    config.hctrl_mode = PCNT_MODE_KEEP;
    // Дія на фронті LOW → HIGH імпульсного входу
    config.pos_mode = PCNT_COUNT_INC;
    // Дія на фронті HIGH → LOW імпульсного входу.
    config.neg_mode = PCNT_COUNT_DEC;
    // Верхня межа; при її досягненні лічильник скидається в 0.
    config.counter_h_lim = COUNTER_LIMIT;
    // Нижня межа; при її досягненні лічильник скидається в 0.
    config.counter_l_lim = -COUNTER_LIMIT;
    config.unit = ENCODER_UNIT;
    config.channel = PCNT_CHANNEL_0;
    ESP_ERROR_CHECK(pcnt_unit_config(&config));
    ESP_ERROR_CHECK(pcnt_counter_pause(ENCODER_UNIT));

    // Канал 1: фронти DT, напрямок залежить від рівня CLK.
    // Протилежні режими дають той самий знак на всіх 4 переходах.
    config.pulse_gpio_num = PIN_DT;
    config.ctrl_gpio_num = PIN_CLK;
    config.pos_mode = PCNT_COUNT_DEC;
    config.neg_mode = PCNT_COUNT_INC;
    config.channel = PCNT_CHANNEL_1;
    ESP_ERROR_CHECK(pcnt_unit_config(&config));

    pinMode(PIN_CLK, INPUT_PULLUP);
    pinMode(PIN_DT, INPUT_PULLUP);

    ESP_ERROR_CHECK(pcnt_counter_clear(ENCODER_UNIT));
    ESP_ERROR_CHECK(pcnt_counter_resume(ENCODER_UNIT));
}

void setup()
{
    Serial0.begin(115200);

    button.attach(PIN_SW, INPUT_PULLUP);
    button.interval(20);
    button.setPressedState(LOW);
    setupEncoder();

    Serial0.println("PCNT encoder: CLK=4, DT=5, SW=6");
    Serial0.println("x4 decoding: 4 counts per full quadrature cycle.");
    Serial0.println("At +/-30000 counts hardware resets to 0 (no overflow extension).");
    Serial0.println("Press SW to reset. PCNT = 0 | Steps = 0");
}

void loop()
{
    button.update();
    if (button.pressed())
    {
        ESP_ERROR_CHECK(pcnt_counter_pause(ENCODER_UNIT));
        ESP_ERROR_CHECK(pcnt_counter_clear(ENCODER_UNIT));
        ESP_ERROR_CHECK(pcnt_counter_resume(ENCODER_UNIT));
        previousCount = 0;
        Serial0.println("Button pressed: PCNT = 0 | Steps = 0");
    }

    // GPIO енкодера тут не опитуються: PCNT рахує самостійно.
    // Читаємо готове значення не частіше ніж раз на 50 мс для консолі.
    const uint32_t now = millis();
    if (now - lastPrintTime < PRINT_INTERVAL_MS)
    {
        return;
    }
    lastPrintTime = now;

    int16_t count = 0;
    ESP_ERROR_CHECK(pcnt_get_counter_value(ENCODER_UNIT, &count));
    if (count != previousCount)
    {
        Serial0.printf("%s | PCNT = %d | Steps = %d\n",
                      count > previousCount ? "CW (+)" : "CCW (-)",
                      static_cast<int>(count), count / COUNTS_PER_STEP);
        previousCount = count;
    }
    
    // delay();
}