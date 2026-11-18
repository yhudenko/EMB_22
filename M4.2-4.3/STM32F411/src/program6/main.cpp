// ############################################################################
// #                              !!! WARNING !!!                             #
// #  DS1307 module is powered from 5V, but STM32 I2C pins are 3.3V logic.    #
// #  The module's onboard pull-up resistors (R2, R3) go to 5V and MUST be    #
// #  REMOVED. Use EXTERNAL pull-ups (e.g. 4.7k) on SDA and SCL to 3.3V.      #
// #  Otherwise 5V on SDA/SCL can damage the MCU (PB6/PB7 are not 5V-safe     #
// #  in all configurations).                                                 #
// #                                                                          #
// #  BATTERY: module has a charging circuit for LIR2032 (rechargeable).      #
// #  To use a simple NON-rechargeable CR2032: REMOVE D1 and R4, and SHORT    #
// #  R6 (solder bridge). Otherwise the cell is charged and may leak/explode. #
// ############################################################################
//
// Demo: DS1307 RTC set/get time over I2C1 (Arduino, no external libs).
//
// Wiring: SDA -> PB7, SCL -> PB6, VCC -> 5V, GND -> GND
// Serial (USART1 PA9/PA10, 115200): type "set YYYY-MM-DD HH:MM:SS" to set time.
// Time is printed once per second.

#include <Arduino.h>
#include <Wire.h>

#define DS1307_ADDR 0x68

TwoWire I2C1Bus(PB7, PB6);

struct DateTime {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
};

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Read time registers 0x00..0x06
bool getTime(DateTime &dt)
{
    I2C1Bus.beginTransmission(DS1307_ADDR);
    I2C1Bus.write((uint8_t)0x00);
    if (I2C1Bus.endTransmission() != 0) return false;
    if (I2C1Bus.requestFrom((uint8_t)DS1307_ADDR, (uint8_t)7) != 7) return false;

    uint8_t sec = I2C1Bus.read();
    dt.second = bcd2bin(sec & 0x7F);  // bit7 = CH (clock halt)
    dt.minute = bcd2bin(I2C1Bus.read() & 0x7F);
    dt.hour = bcd2bin(I2C1Bus.read() & 0x3F);  // 24h mode
    I2C1Bus.read();                            // day of week, unused
    dt.day = bcd2bin(I2C1Bus.read() & 0x3F);
    dt.month = bcd2bin(I2C1Bus.read() & 0x1F);
    dt.year = 2000 + bcd2bin(I2C1Bus.read());
    return !(sec & 0x80);  // false if oscillator halted
}

// Write time; clearing CH bit in seconds starts the oscillator
bool setTime(const DateTime &dt)
{
    I2C1Bus.beginTransmission(DS1307_ADDR);
    I2C1Bus.write((uint8_t)0x00);
    I2C1Bus.write(bin2bcd(dt.second) & 0x7F); // 128 = 2^7 // 0 (Clock Halt) 0000000 (data)
    I2C1Bus.write(bin2bcd(dt.minute));
    I2C1Bus.write(bin2bcd(dt.hour));
    I2C1Bus.write((uint8_t)1);  // day of week, not computed
    I2C1Bus.write(bin2bcd(dt.day));
    I2C1Bus.write(bin2bcd(dt.month));
    I2C1Bus.write(bin2bcd(dt.year % 100));
    return I2C1Bus.endTransmission() == 0;
}

// Parse "set YYYY-MM-DD HH:MM:SS"
void handleCommand(const char *line)
{
    int y, mo, d, h, mi, s;
    if (sscanf(line, "set %d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6 &&
        mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && h < 24 && mi < 60 && s < 60 && y >= 2000 && y <= 2099) {
        DateTime dt = {(uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
        Serial.println(setTime(dt) ? "Time set" : "I2C error");
    } else {
        Serial.println("Usage: set YYYY-MM-DD HH:MM:SS");
    }
}

void setup()
{
    Serial.begin(115200);
    I2C1Bus.begin();
    Serial.println("DS1307 demo. Usage: set YYYY-MM-DD HH:MM:SS");
}

void loop()
{
    // Once the user starts typing (lineLen > 0), stay here until Enter.
    // Time printing is paused while a command is being typed.
    static char lineBuf[48];
    static size_t lineLen = 0;
    do {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\r' || c == '\n') {
                if (lineLen > 0) {
                    Serial.println();
                    lineBuf[lineLen] = '\0';
                    lineLen = 0;
                    handleCommand(lineBuf);
                }
            } else if ((c == '\b' || c == 0x7F) && lineLen > 0) {
                lineLen--;  // backspace: erase on terminal
                Serial.print("\b \b");
            } else if (lineLen < sizeof(lineBuf) - 1 && c >= ' ') {
                lineBuf[lineLen++] = c;  // store + echo
                Serial.write(c);
            }
        }
    } while (lineLen > 0);

    // Print time once per second
    static uint32_t last = 0;
    if (millis() - last >= 1000) {
        last = millis();
        DateTime dt;
        if (getTime(dt)) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
            Serial.println(buf);
        } else {
            Serial.println("DS1307 not found or clock halted (set time)");
        }
    }
}
