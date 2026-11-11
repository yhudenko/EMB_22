#include <Arduino.h>

// Bridge between ESP32 (USART2) and two PC serial ports.
//
// USART2 (PA2 = TX, PA3 = RX) <-> ESP32 (cross: STM TX -> ESP RX, STM RX -> ESP TX, common GND)
// USART1 (PA9 = TX, PA10 = RX) <-> ST-Link VCP
// USB-C CDC (PA11/PA12)        <-> PC (Serial)
//
// ESP32 -> forwarded to USART1 and USB-C
// USART1 or USB-C -> forwarded to ESP32

constexpr uint8_t LED_PIN = PC13;  // active LOW
constexpr uint32_t BAUD = 115200;

Uart SerialESP(PA3, PA2);  // RX, TX
Uart SerialVCP(PA10, PA9);

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    Serial.begin(BAUD);  // USB CDC
    SerialVCP.begin(BAUD);
    SerialESP.begin(BAUD);

    SerialVCP.println("program5: ESP32 bridge ready");
}

void loop()
{
    // Heartbeat so USB output is visible whenever the monitor connects
    static uint32_t lastBeat = 0;
    if (millis() - lastBeat >= 2000) {
        lastBeat = millis();
        if (Serial) {
            Serial.println("[usb alive]");
        }
    }

    // Send a message to the ESP32 every second
    static uint32_t lastSend = 0;
    static uint32_t counter = 0;
    if (millis() - lastSend >= 1000) {
        lastSend = millis();
        SerialESP.print("STM32 msg #");
        SerialESP.println(counter++);
    }

    // ESP32 -> USART1 + USB-C
    while (SerialESP.available()) {
        uint8_t c = SerialESP.read();
        SerialVCP.write(c);
        if (Serial) {
            Serial.write(c);
        }
        digitalToggle(LED_PIN);
    }

    // USART1 -> ESP32
    while (SerialVCP.available()) {
        SerialESP.write(SerialVCP.read());
    }

    // USB-C -> ESP32
    while (Serial.available()) {
        SerialESP.write(Serial.read());
    }
}
