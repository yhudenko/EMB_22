#include <Arduino.h>

// Black Pill STM32F411CEU6: onboard LED on PC13 (active LOW)
constexpr uint8_t LED_PIN = PC13;

// USART1: PA9 = TX, PA10 = RX (to ST-Link VCP)
// Serial Virtual COM Port
Uart SerialVCP(PA10, PA9);

void setup() {
  pinMode(LED_PIN, OUTPUT);
  SerialVCP.begin(115200);
}

void loop() {

  digitalWrite(LED_PIN, LOW);   // LED on
  SerialVCP.println("Hello from STM32F411! LED ON");
  delay(500);
  digitalWrite(LED_PIN, HIGH);  // LED off
  SerialVCP.println("Hello from STM32F411! LED OFF");
  delay(500);
}
