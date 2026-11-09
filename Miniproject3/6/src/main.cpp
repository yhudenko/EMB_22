#include "SafeLogic.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

SafeLogic safe;

extern "C" void app_main(void) {
    safe.init();
    while (true) {
        safe.update();
        vTaskDelay(1);
    }
}
