#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

// SSD1306 128x64 OLED on I2C1: SDA = PB7, SCL = PB6, address 0x3C
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C

TwoWire I2C1Bus(PB7, PB6);
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &I2C1Bus, -1);

// 16x16 heart icon
const uint8_t heartIcon[] PROGMEM = {0x00, 0x00, 0x1C, 0x38, 0x3E, 0x7C, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
                                     0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 16x16 smiley icon
const uint8_t smileIcon[] PROGMEM = {0x07, 0xE0, 0x18, 0x18, 0x20, 0x04, 0x40, 0x02, 0x46, 0x62, 0x86, 0x61, 0x80, 0x01, 0x80, 0x01,
                                     0x80, 0x01, 0x90, 0x09, 0x48, 0x12, 0x47, 0xE2, 0x20, 0x04, 0x18, 0x18, 0x07, 0xE0, 0x00, 0x00};

const uint8_t Image20261[] PROGMEM = {
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x40, 0x01, 0xc0, 0x01, 0xc0, 0x00, 0xc0, 0x01, 0xc0, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
                                     
void showText()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Size 1: Hello OLED");
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.println("Size 2");
    display.setTextSize(3);
    display.setCursor(0, 36);
    display.println("Big");
    display.display();
    delay(2500);
}

void showIcons()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Icons");
    display.drawBitmap(10, 20, heartIcon, 16, 16, SSD1306_WHITE);
    display.drawBitmap(40, 20, smileIcon, 16, 16, SSD1306_WHITE);
    display.drawRect(70, 20, 16, 16, SSD1306_WHITE);
    display.fillCircle(104, 28, 8, SSD1306_WHITE);
    display.drawLine(0, 50, 127, 63, SSD1306_WHITE);
    display.display();
    delay(2500);
}

void showUptime()
{
    for (int i = 0; i < 20; i++) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("STM32F411 BlackPill");
        display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
        display.setTextSize(2);
        display.setCursor(0, 20);
        display.print(millis() / 1000);
        display.println(" s");
        // progress bar
        display.drawRect(0, 48, 128, 12, SSD1306_WHITE);
        display.fillRect(2, 50, (i + 1) * 124 / 20, 8, SSD1306_WHITE);
        display.display();
        delay(150);
    }
}

void setup()
{
    I2C1Bus.begin();
    while (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        delay(1000);
    }
    display.clearDisplay();
    display.display();
}

void loop()
{
    showText();
    showIcons();
    showUptime();
}
