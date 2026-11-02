#include "time_base.h"

namespace {

hw_timer_t* g_timer = nullptr;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t g_tickMs = 0;

void IRAM_ATTR onTick() {
    portENTER_CRITICAL_ISR(&g_mux);
    ++g_tickMs;
    portEXIT_CRITICAL_ISR(&g_mux);
}

}  // namespace

namespace TimeBase {

void begin() {
    // 80 MHz / 80 = 1 MHz (1 us tick); alarm every 1000 us = 1 ms.
    g_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(g_timer, &onTick, true);
    timerAlarmWrite(g_timer, 1000, true);
    timerAlarmEnable(g_timer);
}

uint32_t takeElapsedMs() {
    portENTER_CRITICAL(&g_mux);
    const uint32_t ms = g_tickMs;
    g_tickMs = 0;
    portEXIT_CRITICAL(&g_mux);
    return ms;
}

}  // namespace TimeBase
