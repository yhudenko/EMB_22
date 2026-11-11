#include <Arduino.h>
#include <STM32FreeRTOS.h>

// Black Pill STM32F411: LED on PC13 (active LOW)
// USART1: PA9 = TX, PA10 = RX (to ST-Link VCP)
constexpr uint8_t LED_PIN = PC13;
Uart SerialVCP(PA10, PA9);

static QueueHandle_t msgQueue;

static void ledTask(void *) {
  uint32_t count = 0;
  for (;;) {
    digitalToggle(LED_PIN);
    count++;
    xQueueSend(msgQueue, &count, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Only this task writes to the UART
static void uartTask(void *) {
  uint32_t count;
  SerialVCP.println("program4: Arduino + FreeRTOS started");
  for (;;) {
    if (xQueueReceive(msgQueue, &count, portMAX_DELAY) == pdTRUE) {
      SerialVCP.print("LED toggle #");
      SerialVCP.print(count);
      SerialVCP.print(" (tick ");
      SerialVCP.print(xTaskGetTickCount());
      SerialVCP.println(")");
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  SerialVCP.begin(115200);

  msgQueue = xQueueCreate(8, sizeof(uint32_t));

  xTaskCreate(ledTask, "led", 128, nullptr, 1, nullptr);
  xTaskCreate(uartTask, "uart", 256, nullptr, 2, nullptr);

  vTaskStartScheduler();
}

void loop() {}
