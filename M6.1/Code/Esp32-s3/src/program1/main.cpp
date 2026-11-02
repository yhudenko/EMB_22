#include <Arduino.h>
#include <Bounce2.h>

// ---------------------------------------------------------------------------
// Pin configuration (see pins.txt for safe GPIOs on this board)
// ---------------------------------------------------------------------------
static const gpio_num_t LED_PIN    = GPIO_NUM_2;   // normal GPIO, safe to use
static const gpio_num_t BUTTON_PIN = GPIO_NUM_4;   // button to GND, uses pull-up

// ---------------------------------------------------------------------------
// FreeRTOS handles (created in setup, used by the tasks)
// ---------------------------------------------------------------------------
static SemaphoreHandle_t serialMutex    = nullptr;  // protects Serial (shared)
static SemaphoreHandle_t buttonSemaphore = nullptr;  // signalled on a press

// Debouncer for the push button (Bounce2 library).
static Bounce button = Bounce();

// A shared counter guarded by the mutex, written by two tasks.
static volatile uint32_t sharedCounter = 0;

// ---------------------------------------------------------------------------
// Helper: thread-safe Serial print.
// Serial is a shared resource; without the mutex two tasks printing at the same
// time would interleave their characters. We take the mutex, print, give back.
// ---------------------------------------------------------------------------
static void safePrint(const char *msg)
{
    // Block up to 100 ms waiting for the mutex to become free.
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        Serial.println(msg);
        xSemaphoreGive(serialMutex);   // always release what you take
    }
}

// ---------------------------------------------------------------------------
// Poll task: debounces the button with Bounce2 and gives the semaphore once
// per confirmed press. Polling every ~5 ms is plenty for a human button and
// keeps the ISR out of the picture (Bounce2 is a polling debouncer).
// ---------------------------------------------------------------------------
static void buttonPollTask(void *param)
{
    for (;;)
    {
        button.update();
        // fell() is true for one update after a confirmed HIGH->LOW transition
        // (button pressed, because of INPUT_PULLUP wiring to GND).
        if (button.fell())
        {
            xSemaphoreGive(buttonSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ---------------------------------------------------------------------------
// Task 1: blink the LED using vTaskDelay.
// vTaskDelay blocks the task (lets the CPU run others) instead of busy-waiting
// like delay()/millis() loops would.
// ---------------------------------------------------------------------------
static void blinkTask(void *param)
{
    const TickType_t xFrequency = 100;
    pinMode(LED_PIN, OUTPUT);
    bool on = false;
    for (;;) // while(true)
    {
        auto xLastWakeTime = xTaskGetTickCount();

        on = !on;
        digitalWrite(LED_PIN, on ? HIGH : LOW);
        // Delay 500 ms. pdMS_TO_TICKS converts milliseconds to RTOS ticks.
        //vTaskDelay(pdMS_TO_TICKS(500));
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ---------------------------------------------------------------------------
// Task 2 & 3: both increment the shared counter under the mutex.
// This shows mutual exclusion: only one task touches sharedCounter at a time.
// The task name is passed in via the parameter.
// ---------------------------------------------------------------------------
static void counterTask(void *param)
{
    const char *name = static_cast<const char *>(param);
    char buffer[64];

    for (;;)
    {
        // Critical section protected by the mutex.
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
        {
            uint32_t value = ++sharedCounter;
            snprintf(buffer, sizeof(buffer), "[%s] counter = %lu",
                     name, static_cast<unsigned long>(value));
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }

        // Different cadence per task so you can see them interleave.
        vTaskDelay(pdMS_TO_TICKS(name[0] == 'A' ? 700 : 1100));
    }
}

// ---------------------------------------------------------------------------
// Task 4: waits for the button semaphore. Blocks (0% CPU) until the ISR signals.
// ---------------------------------------------------------------------------
static void buttonTask(void *param)
{
    for (;;)
    {
        // Wait forever until the poll task gives the semaphore.
        if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE)
        {
            safePrint(">>> Button pressed! (debounced, handled in task)");
        }
    }
}

// ---------------------------------------------------------------------------
// setup(): create the RTOS objects and the tasks, then return.
// The scheduler keeps running the tasks after setup()/loop() exist.
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(300);                       // small startup settle time
    Serial.println("\nFreeRTOS demo starting...");

    // --- Create synchronization primitives ---------------------------------
    serialMutex    = xSemaphoreCreateMutex();   // mutex for the shared Serial
    buttonSemaphore = xSemaphoreCreateBinary();  // binary semaphore for the ISR

    if (serialMutex == nullptr || buttonSemaphore == nullptr)
    {
        Serial.println("Failed to create RTOS objects!");
        return;
    }

    // --- Button + debouncer -------------------------------------------------
    button.attach(BUTTON_PIN, INPUT_PULLUP);  // sets pinMode internally
    button.interval(25);                       // 25 ms debounce window

    // --- Create tasks -------------------------------------------------------
    // xTaskCreate( fn, name, stackBytes, param, priority, handle )
    xTaskCreate(blinkTask,      "blink",  2048, nullptr, 1, nullptr);
    xTaskCreate(buttonTask,     "button", 2048, nullptr, 3, nullptr);
    xTaskCreate(buttonPollTask, "btnPoll",2048, nullptr, 3, nullptr);

    // Two counter tasks sharing the same function but different names.
    // Pinned to specific cores to show xTaskCreatePinnedToCore (ESP32 has 2 cores).
    xTaskCreatePinnedToCore(counterTask, "cntA", 2048, (void *)"A", 2, nullptr, 0);
    xTaskCreatePinnedToCore(counterTask, "cntB", 2048, (void *)"B", 2, nullptr, 1);

    safePrint("Setup complete. Tasks are running.");
}

// ---------------------------------------------------------------------------
// loop(): on Arduino-ESP32 this runs as its own low-priority task.
// We don't need it here, so just sleep to avoid wasting CPU.
// ---------------------------------------------------------------------------
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
