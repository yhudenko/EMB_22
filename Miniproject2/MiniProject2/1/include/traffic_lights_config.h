#pragma once

#define RED_LED_PIN 48
#define YELLOW_LED_PIN 47
#define GREEN_LED_PIN 21

#define GREEN_MASK 0b001
#define YELLOW_MASK 0b010
#define RED_MASK 0b100
#define BLANK_MASK 0b000

#define GREEN_DURATION_MS 40000
#define GREEN_BLINK_DURATION_MS 3000
#define YELLOW_DURATION_MS 3000
#define RED_DURATION_MS 110000
#define RED_YELLOW_DURATION_MS 3000
#define TICK_MS 500
#define BLINK_PERIOD_MS 500

#define DIVIDER 80
#define ALARM_VALUE 500000

enum State {
    GREEN,
    GREEN_BLINK,
    YELLOW,
    RED,
    RED_YELLOW,
};

enum LightCycleTime {
    GREEN_TIME = GREEN_DURATION_MS + GREEN_BLINK_DURATION_MS,
    YELLOW_TIME = YELLOW_DURATION_MS,
    RED_TIME = RED_DURATION_MS + RED_YELLOW_DURATION_MS,
};

struct StateInfo {
    uint32_t ticksMs;
    State next;
    boolean breakTimerOnChange;
    uint32_t time;
};

static const StateInfo STATES[] = {
    {.ticksMs = GREEN_DURATION_MS, .next = GREEN_BLINK, .breakTimerOnChange = false, .time = GREEN_TIME}, // GREEN
    {.ticksMs = GREEN_BLINK_DURATION_MS, .next = YELLOW, .breakTimerOnChange = true, .time = GREEN_TIME}, // GREEN_BLINK
    {.ticksMs = YELLOW_DURATION_MS, .next = RED, .breakTimerOnChange = true, .time = YELLOW_TIME}, // YELLOW
    {.ticksMs = RED_DURATION_MS, .next = RED_YELLOW, .breakTimerOnChange = false, .time = RED_TIME}, // RED
    {.ticksMs = RED_YELLOW_DURATION_MS, .next = GREEN, .breakTimerOnChange = true, .time = RED_TIME}, // RED_YELLOW
};