#include <Arduino.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

// ---------------------------------------------------------------------------
// program3 — FreeRTOS queue sample (send BY REFERENCE / by pointer).
//
// When an item is large (e.g. a 1 KB buffer, or a struct holding an array),
// copying it into the queue on every send is expensive in time and memory.
// Instead we allocate the data on the heap and send ONLY its address.
//
// The queue is therefore created with item size = sizeof(DataPacket_t *)
// (a pointer, 4 bytes on ESP32-S3), not sizeof(DataPacket_t).
//
// MEMORY OWNERSHIP RULE:
//   whoever owns the pointer frees it. Ownership is handed over together with
//   the pointer when xQueueSend SUCCEEDS:
//     * send succeeded -> the receiver now owns it and frees it after use.
//     * send FAILED     -> the sender still owns it and must free it itself.
//   These two paths are mutually exclusive, so each packet is freed exactly
//   once (no double free). We also NULL the locals after free as a safety net.
// ---------------------------------------------------------------------------

static const char *TAG_TX = "TX";
static const char *TAG_RX = "RX";


// typedef unsigned int uint;

typedef struct
{
    uint8_t * buffer;     // heap buffer with the payload
    size_t   length;     // payload length in bytes
    uint32_t timestamp;  // tick count when the packet was produced
} DataPacket_t;

// Queue sized for a POINTER, not for the whole packet.
static QueueHandle_t xPtrQueue = nullptr;

static const UBaseType_t QUEUE_LENGTH  = 5;
static const size_t      PAYLOAD_BYTES = 256;

// ---------------------------------------------------------------------------
// Helper: free a packet and everything it owns, then NULL the caller's pointer.
// Centralising the free logic guarantees buffer and struct are released in the
// right order and keeps both the success and failure paths consistent.
// ---------------------------------------------------------------------------
static void freePacket(DataPacket_t **ppPacket)
{
    if (ppPacket == nullptr || *ppPacket == nullptr)
    {
        return;
    }

    DataPacket_t *packet = *ppPacket;
    free(packet->buffer);   // free inner buffer first
    packet->buffer = nullptr;
    free(packet);           // then the struct itself
    *ppPacket = nullptr;    // caller's pointer no longer dangles
}

// ---------------------------------------------------------------------------
// Sender task: allocate a packet on the heap, send its ADDRESS into the queue.
// Only 4 bytes (the pointer) are copied, not the 256+ bytes of payload.
// ---------------------------------------------------------------------------
static void sender_task(void *pvParameters)
{
    while (1)
    {
        // Allocate the packet struct on the heap.
        DataPacket_t *packet = (DataPacket_t *)malloc(sizeof(DataPacket_t));
        configASSERT(packet != NULL);           // always check allocations

        // Allocate its payload buffer on the heap.
        packet->buffer = (uint8_t *)malloc(PAYLOAD_BYTES);
        configASSERT(packet->buffer != NULL);

        packet->length    = PAYLOAD_BYTES;
        packet->timestamp = xTaskGetTickCount();

        // Only the pointer (4 bytes) is copied into the queue — not the buffer.
        if (xQueueSend(xPtrQueue, &packet, pdMS_TO_TICKS(100)) == pdPASS)
        {
            // Success: ownership transferred to the receiver. Do NOT free here,
            // and drop our local reference so we can never touch it again.
            packet = nullptr;
        }
        else
        {
            // Queue full: the receiver will never see this packet, so we are
            // still the owner and must free it right here.
            ESP_LOGW(TAG_TX, "Queue full, packet dropped");
            freePacket(&packet);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------------------------------------------------------------------
// Receiver task: block until a pointer arrives, process the data, then free it.
// ---------------------------------------------------------------------------
static void receiver_task(void *pvParameters)
{
    DataPacket_t *packet = nullptr;

    while (1)
    {
        if (xQueueReceive(xPtrQueue, &packet, portMAX_DELAY) == pdPASS)
        {
            // We now own the packet. Process the data...
            ESP_LOGI(TAG_RX, "Len: %u, T: %lu",
                     (unsigned)packet->length,
                     (unsigned long)packet->timestamp);

            // ...then release it. Ownership ends here, exactly one free().
            freePacket(&packet);
        }
    }
}

// ---------------------------------------------------------------------------
// setup(): create the pointer-sized queue and the two tasks.
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("FreeRTOS queue (by pointer) demo starting...");

    // Item size is sizeof(DataPacket_t *) — a pointer, not the whole packet.
    xPtrQueue = xQueueCreate(QUEUE_LENGTH, sizeof(DataPacket_t *));
    if (xPtrQueue == nullptr)
    {
        Serial.println("Failed to create queue!");
        return;
    }

    xTaskCreate(sender_task,   "sender",   4096, nullptr, 2, nullptr);
    xTaskCreate(receiver_task, "receiver", 4096, nullptr, 2, nullptr);

    Serial.println("Setup complete. Tasks are running.");
}

// ---------------------------------------------------------------------------
// loop(): unused, just sleep so it does not waste CPU.
// ---------------------------------------------------------------------------
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
