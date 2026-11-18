// Demo: BME280 + SSD1306 128x64 OLED on the same I2C1 bus (Arduino + libs).
//
// Wiring (both devices share the bus):
//   SDA -> PB7, SCL -> PB6, VCC -> 3V3, GND -> GND
//   BME280 address 0x76 (SDO -> GND), OLED address 0x3C

#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#define BME_ADDR 0x76
#define OLED_ADDR 0x3C

TwoWire I2C1Bus(PB7, PB6);
Adafruit_SSD1306 display(128, 64, &I2C1Bus, -1);
Adafruit_BME280 bme;

// Serial is mapped to USART1 (PA9 TX / PA10 RX) via -DSERIAL_UART_INSTANCE=1 in platformio.ini

void setup()
{
    Serial.begin(115200);
    I2C1Bus.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED not found");
        while (true) {
        }
    }
    if (!bme.begin(BME_ADDR, &I2C1Bus)) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("BME280 not found");
        display.display();
        while (true) {
        }
    }
}

void loop()
{
    float t = bme.readTemperature();        // degC
    float p = bme.readPressure() / 100.0f;  // hPa
    float h = bme.readHumidity();           // %RH

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("BME280");

    display.setTextSize(2);
    display.setCursor(0, 14);
    display.print(t, 1);
    display.println(" C");
    display.setCursor(0, 32);
    display.print(h, 1);
    display.println(" %");

    display.setTextSize(1);
    display.setCursor(0, 54);
    display.print(p, 1);
    display.print(" hPa");
    display.display();

    Serial.printf("T=%.2f C P=%.2f hPa H=%.2f %%\r\n", t, p, h);
    delay(1000);
}
