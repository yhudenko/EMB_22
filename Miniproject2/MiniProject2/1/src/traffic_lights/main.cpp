#include <Arduino.h>
#include <seven_segment_config.h>
#include <traffic_lights_config.h>

#include "display_helper.h"
#include "leds_helper.h"

static hw_timer_t *led_timer = nullptr;

static volatile uint32_t led_timer_count = 0;
static volatile uint32_t display_timer_count = 0;
static volatile uint32_t lastScanUs = 0;
static uint32_t displayRemainingMs = 0;

static volatile State current_mode = GREEN;
static volatile uint8_t current_mask = GREEN_MASK;
static volatile bool mask_changed = false;

static uint8_t activeDisplay = 0;

static IRAM_ATTR uint8_t getModeMask(State mode) {
    switch (mode) {
        case GREEN:
            return GREEN_MASK;
        case GREEN_BLINK:
            return (led_timer_count % 1000 != 0) ? GREEN_MASK : BLANK_MASK;
        case YELLOW:
            return YELLOW_MASK;
        case RED:
            return RED_MASK;
        case RED_YELLOW:
            return RED_MASK | YELLOW_MASK;
        default:
            return BLANK_MASK;
    }
    return BLANK_MASK;
}

static void IRAM_ATTR onTimer() {
    led_timer_count += TICK_MS;
    display_timer_count += TICK_MS;
    if (led_timer_count >= STATES[current_mode].ticksMs) {
        if (STATES[current_mode].breakTimerOnChange) {
            display_timer_count = 0;
        }
        led_timer_count = 0;
        current_mode = STATES[current_mode].next;
    }
    uint8_t new_mask = getModeMask(current_mode);
    if (new_mask != current_mask) {
        current_mask = new_mask;
        mask_changed = true;
    }

    if (display_timer_count % displayUpdateIntervalMs == 0) {
        displayRemainingMs = STATES[current_mode].time - display_timer_count;
    }

}

void setup() {
    Serial.begin(9600);
    delay(1000);

    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);

    led_timer = timerBegin(0, DIVIDER, true);

    timerAttachInterrupt(led_timer, &onTimer, true);
    timerAlarmWrite(led_timer, ALARM_VALUE, true);
    timerAlarmEnable(led_timer);

    writeLeds(current_mask);

    setPins(kDisplayEnablePins, 2, LOW);
    setPins(kSegmentPins, 7, HIGH);

    pinMode(kDecimalPointPin, OUTPUT);
    digitalWriteReverse(kDecimalPointPin, LOW);

    disableAllDisplays();
}

void loop() {
    if (mask_changed) {
        mask_changed = false;
        writeLeds(current_mask);
    }

    const uint32_t nowUs = micros();
    if (nowUs - lastScanUs >= digitScanIntervalUs) {
        lastScanUs = nowUs;
        uint32_t currentValue = msToSeconds(displayRemainingMs);

        int activeDigit = -1;
        if (currentValue <= 99) {
            const uint8_t firstDigit = currentValue / 10;
            const uint8_t secondDigit = currentValue % 10;
            activeDigit = activeDisplay == 0 ? firstDigit : secondDigit;
        }
        showDigit(activeDisplay, activeDigit);

        activeDisplay = (activeDisplay + 1) % 2;

    }
}
