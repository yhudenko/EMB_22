#include <Arduino.h>

// Serial1 (GPIO17 = TX, GPIO18 = RX) <-> STM32 Black Pill USART2 (PA3 = RX, PA2 = TX)

constexpr int UART_RX_PIN = 18;
constexpr int UART_TX_PIN = 17;
constexpr uint32_t BAUD = 115200;

void setup()
{
    Serial0.begin(BAUD);
    Serial1.begin(BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    delay(1000);
    Serial0.println("esp32-s3 repeater ready");
}

void loop()
{
    // STM32 -> USB-C
    while (Serial1.available()) {
        Serial0.write(Serial1.read());
    }

    // USB-C -> STM32
    while (Serial0.available()) {
        Serial1.write(Serial.read());
    }
}
