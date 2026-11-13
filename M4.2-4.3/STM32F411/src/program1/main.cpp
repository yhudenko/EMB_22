#include <Arduino.h>
#include <Wire.h>

Uart SerialVCP(PA10, PA9);  // RX, TX

// I2C1: SCL = PB6, SDA = PB7
constexpr uint8_t SCL1_PIN = PB6;
constexpr uint8_t SDA1_PIN = PB7;

TwoWire I2C1Bus(SDA1_PIN, SCL1_PIN);

void setup()
{
    SerialVCP.begin(115200);
    delay(1000);
    I2C1Bus.begin();
    SerialVCP.println("I2C Scanner (SDA=PB7, SCL=PB6)");
}

void loop()
{
    uint8_t count = 0;
    SerialVCP.println("Scanning...");

    for (uint8_t addr = 1; addr < 127; addr++) {
        I2C1Bus.beginTransmission(addr);
        uint8_t err = I2C1Bus.endTransmission();
        if (err == 0) {
            SerialVCP.print("Device found at 0x");
            if (addr < 16) {
                SerialVCP.print('0');
            }
            SerialVCP.println(addr, HEX);
            count++;
        }
        else if (err == 4) {
            SerialVCP.print("Unknown error at 0x");
            if (addr < 16) {
                SerialVCP.print('0');
            }
            SerialVCP.println(addr, HEX);
        }
    }

    if (count == 0) {
        SerialVCP.println("No I2C devices found");
    }
    else {
        SerialVCP.print("Devices found: ");
        SerialVCP.println(count);
    }
    SerialVCP.println();
    delay(3000);
}
