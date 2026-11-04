#include <Arduino.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

// Log tags for producer and consumer.
static const char *TAG_TX = "TX";
static const char *TAG_RX = "RX";

// ---------------------------------------------------------------------------
// program2 — basic FreeRTOS queue sample (send BY VALUE).
//
// A queue is a thread-safe FIFO. One (or more) producer tasks push items into
// it, one (or more) consumer tasks pop them out. The queue copies the whole
// item in and out, so it is a good fit for SMALL items (a few bytes).
//
// Here the item is a small struct. Because it is small, copying it on every
// send/receive is cheap — that is exactly the case where "send by value" is
// the right choice. (For large items see program3, which sends a pointer.)
// ---------------------------------------------------------------------------

// A small message carried by value through the queue.
typedef struct
{
    uint32_t id;         // sequence number from the producer
    float    value;      // some sampled value
    uint32_t timestamp;  // tick count when it was produced
} SensorSample_t;

// Queue handle, created in setup().
static QueueHandle_t xSampleQueue = nullptr;

// How many items the queue can hold before xQueueSend blocks/fails.
static const UBaseType_t QUEUE_LENGTH = 5;

// ---------------------------------------------------------------------------
// Producer task: creates a sample and sends a COPY of it into the queue.
// If the queue is full it waits up to 100 ms; if still full, it reports a drop.
// ---------------------------------------------------------------------------
static void producerTask(void *param)
{
    uint32_t counter = 0;

    for (;;)
    {
        SensorSample_t sample;
        sample.id        = counter++;
        sample.value     = 20.0f + (float)(esp_random() % 1000) / 100.0f; // ~20..30
        sample.timestamp = xTaskGetTickCount();

        // xQueueOverwrite overwrite last sample if buffer full

        // The whole struct is copied into the queue (send by value).
        if (xQueueSend(xSampleQueue, &sample, pdMS_TO_TICKS(100)) == pdPASS)
        {
            ESP_LOGI(TAG_TX, "sent id=%lu value=%.2f",
                     (unsigned long)sample.id, sample.value);
        }
        else
        {
            // Queue stayed full for 100 ms — nothing to free, the item lived
            // on the stack, so dropping it just means not sending.
            ESP_LOGW(TAG_TX, "queue full, dropped id=%lu",
                     (unsigned long)sample.id);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ---------------------------------------------------------------------------
// Consumer task: blocks until an item is available, then copies it out.
// portMAX_DELAY means "wait forever" — the task uses 0% CPU while waiting.
// ---------------------------------------------------------------------------
static void consumerTask(void *param)
{
    SensorSample_t sample;

    for (;;)
    {

        // xQueuePeek get sample without removing them from Queue
        // while (xQueueReceive(xSampleQueue, &sample, 0) == pdTRUE) get all sample from Queue

        if (xQueueReceive(xSampleQueue, &sample, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG_RX, "got  id=%lu value=%.2f t=%lu",
                     (unsigned long)sample.id,
                     sample.value,
                     (unsigned long)sample.timestamp);

            // Simulate some processing time, slower than the producer so the
            // queue occasionally fills up and you can see "queue full" above.
            vTaskDelay(pdMS_TO_TICKS(350));
        }
    }
}

// ---------------------------------------------------------------------------
// setup(): create the queue and the two tasks.
// ---------------------------------------------------------------------------
void setup()
{
    Serial0.begin(115200);
    delay(1000);
    ESP_LOGI("main", "FreeRTOS queue (by value) demo starting...");

    // xQueueCreate( length, itemSizeInBytes )
    // The item size is the size of the WHOLE struct, because we copy it in/out.
    xSampleQueue = xQueueCreate(QUEUE_LENGTH, sizeof(SensorSample_t));
    if (xSampleQueue == nullptr)
    {
        ESP_LOGE("main", "Failed to create queue!");
        return;
    }

    xTaskCreate(producerTask, "producer", 4096, nullptr, 2, nullptr);
    xTaskCreate(consumerTask, "consumer", 4096, nullptr, 2, nullptr);

    ESP_LOGI("main", "Setup complete. Tasks are running.");
}

// ---------------------------------------------------------------------------
// loop(): unused, just sleep so it does not waste CPU.
// ---------------------------------------------------------------------------
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
