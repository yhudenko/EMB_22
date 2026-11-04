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
static SemaphoreHandle_t Serial0Mutex    = nullptr;  // protects Serial0 (shared)
static SemaphoreHandle_t buttonSemaphore = nullptr;  // signalled on a press

// Task handles, kept so the stats task can query each task's stack usage.
static TaskHandle_t hBlink   = nullptr;
static TaskHandle_t hButton  = nullptr;
static TaskHandle_t hBtnPoll = nullptr;
static TaskHandle_t hCntA    = nullptr;
static TaskHandle_t hCntB    = nullptr;

// Debouncer for the push button (Bounce2 library).
static Bounce button = Bounce();

// A shared counter guarded by the mutex, written by two tasks.
static volatile uint32_t sharedCounter = 0;

// ---------------------------------------------------------------------------
// Helper: thread-safe Serial0 print.
// Serial0 is a shared resource; without the mutex two tasks printing at the same
// time would interleave their characters. We take the mutex, print, give back.
// ---------------------------------------------------------------------------
static void safePrint(const char *msg)
{
    // Block up to 100 ms waiting for the mutex to become free.
    if (xSemaphoreTake(Serial0Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        Serial0.println(msg);
        xSemaphoreGive(Serial0Mutex);   // always release what you take
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
        if (xSemaphoreTake(Serial0Mutex, portMAX_DELAY) == pdTRUE)
        {
            uint32_t value = ++sharedCounter;
            snprintf(buffer, sizeof(buffer), "[%s] counter = %lu",
                     name, static_cast<unsigned long>(value));
            Serial0.println(buffer);
            xSemaphoreGive(Serial0Mutex);
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
// Diagnostic task
// ---------------------------------------------------------------------------
static void statsTask(void *param)
{
    for (;;)
    {
        if (xSemaphoreTake(Serial0Mutex, portMAX_DELAY) == pdTRUE)
        {
            // uxTaskGetStackHighWaterMark returns the MINIMUM free stack ever recorded for
            // a task, measured in WORDS (1 word = 4 bytes on ESP32). A value close to 0
            // means that task is about to overflow its stack and needs a bigger one.
            Serial0.printf(
                "[stats] free stack (words): blink=%u button=%u btnPoll=%u "
                "cntA=%u cntB=%u\n",
                (unsigned)uxTaskGetStackHighWaterMark(hBlink),
                (unsigned)uxTaskGetStackHighWaterMark(hButton),
                (unsigned)uxTaskGetStackHighWaterMark(hBtnPoll),
                (unsigned)uxTaskGetStackHighWaterMark(hCntA),
                (unsigned)uxTaskGetStackHighWaterMark(hCntB));

            // Heap usage: task stacks/TCBs are allocated from the heap, so watch
            // the free heap too. "min ever" is the lowest it has ever dropped to
            // (worst case); if it trends toward 0 you are running out of RAM.
            Serial0.printf("[stats] free heap: %u bytes, min ever: %u\n",
                (unsigned)esp_get_free_heap_size(),
                (unsigned)esp_get_minimum_free_heap_size());
            xSemaphoreGive(Serial0Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---------------------------------------------------------------------------
// setup(): create the RTOS objects and the tasks, then return.
// The scheduler keeps running the tasks after setup()/loop() exist.
// ---------------------------------------------------------------------------
void setup()
{
    Serial0.begin(115200);
    delay(300);                       // small startup settle time
    Serial0.println("FreeRTOS demo starting...");

    // --- Create synchronization primitives ---------------------------------
    Serial0Mutex    = xSemaphoreCreateMutex();   // mutex for the shared Serial0
    buttonSemaphore = xSemaphoreCreateBinary();  // binary semaphore for the ISR

    if (Serial0Mutex == nullptr || buttonSemaphore == nullptr)
    {
        Serial0.println("Failed to create RTOS objects!");
        return;
    }

    // --- Button + debouncer -------------------------------------------------
    button.attach(BUTTON_PIN, INPUT_PULLUP);  // sets pinMode internally
    button.interval(25);                       // 25 ms debounce window

    // --- Create tasks -------------------------------------------------------
    // xTaskCreate( fn, name, stackBytes, param, priority, handle )
    xTaskCreate(blinkTask,      "blink",  2048, nullptr, 1, &hBlink);
    xTaskCreate(buttonTask,     "button", 2048, nullptr, 3, &hButton);
    xTaskCreate(buttonPollTask, "btnPoll",2048, nullptr, 3, &hBtnPoll);

    // Two counter tasks sharing the same function but different names.
    // Pinned to specific cores to show xTaskCreatePinnedToCore (ESP32 has 2 cores).
    xTaskCreatePinnedToCore(counterTask, "cntA", 2048, (void *)"A", 2, &hCntA, 0);
    xTaskCreatePinnedToCore(counterTask, "cntB", 2048, (void *)"B", 2, &hCntB, 1);

    // Diagnostic task: prints stack usage of all tasks every 5 s.
    xTaskCreate(statsTask, "stats", 2560, nullptr, 1, nullptr);

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
