#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

// PC <-> STM32 command interface over USART1 (PA9 = TX, PA10 = RX, ST-Link VCP).
//
// Commands (terminated by Enter):
//   help
//   get            - show all fields
//   get <name>     - show one field
//   set <name> <value>
//
// Fields: blink (ms), led (0/1), value (int), gain (float)

constexpr uint8_t LED_PIN = PC13;  // active LOW
constexpr uint32_t BAUD = 115200;

Uart SerialVCP(PA10, PA9);  // RX, TX

struct Config {
    uint32_t blink;  // LED blink period in ms
    uint8_t led;     // 1 = blinking enabled
    int32_t value;
    float gain;
};

Config cfg = {500, 1, 0, 1.0f};

static char lineBuf[64]; // \0
static size_t lineLen = 0;

static void printField(const char *name)
{
    if (strcmp(name, "blink") == 0) {
        SerialVCP.print("blink = ");
        SerialVCP.println(cfg.blink);
    } else if (strcmp(name, "led") == 0) {
        SerialVCP.print("led = ");
        SerialVCP.println(cfg.led);
    } else if (strcmp(name, "value") == 0) {
        SerialVCP.print("value = ");
        SerialVCP.println(cfg.value);
    } else if (strcmp(name, "gain") == 0) {
        SerialVCP.print("gain = ");
        SerialVCP.println(cfg.gain, 3);
    } else {
        SerialVCP.println("ERR: unknown field");
    }
}

static void setField(const char *name, const char *val)
{
    char *end = nullptr;
    if (strcmp(name, "blink") == 0) {
        long v = strtol(val, &end, 10);
        if (*end != '\0' || v <= 0) { SerialVCP.println("ERR: bad value"); return; }
        cfg.blink = (uint32_t)v;
    } else if (strcmp(name, "led") == 0) {
        long v = strtol(val, &end, 10);
        if (*end != '\0' || (v != 0 && v != 1)) { SerialVCP.println("ERR: bad value (0/1)"); return; }
        cfg.led = (uint8_t)v;
    } else if (strcmp(name, "value") == 0) {
        long v = strtol(val, &end, 10);
        if (*end != '\0') { SerialVCP.println("ERR: bad value"); return; }
        cfg.value = (int32_t)v;
    } else if (strcmp(name, "gain") == 0) {
        float v = strtof(val, &end);
        if (*end != '\0') { SerialVCP.println("ERR: bad value"); return; }
        cfg.gain = v;
    } else {
        SerialVCP.println("ERR: unknown field");
        return;
    }
    SerialVCP.print("OK ");
    printField(name);
}

static void handleLine(char *line)
{
    char *cmd = strtok(line, " \t");
    if (!cmd) return;
    char *arg1 = strtok(nullptr, " \t");
    char *arg2 = strtok(nullptr, " \t");

    if (strcmp(cmd, "help") == 0) {
        SerialVCP.println("Commands: help | get [name] | set <name> <value>");
        SerialVCP.println("Fields: blink(ms) led(0/1) value(int) gain(float)");
    } else if (strcmp(cmd, "get") == 0) {
        if (arg1) {
            printField(arg1);
        } else {
            printField("blink");
            printField("led");
            printField("value");
            printField("gain");
        }
    } else if (strcmp(cmd, "set") == 0) {
        if (arg1 && arg2) setField(arg1, arg2);
        else SerialVCP.println("ERR: usage: set <name> <value>");
    } else {
        SerialVCP.println("ERR: unknown command (try help)");
    }
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    SerialVCP.begin(BAUD);
    SerialVCP.println("ready");
}

void loop()
{
    while (SerialVCP.available()) {
        char c = (char)SerialVCP.read();
        if (c == '\r' || c == '\n') {
            // Enter: terminate string and run the command (ignore empty lines)
            if (lineLen > 0) {
                SerialVCP.println();
                lineBuf[lineLen] = '\0'; //get\0
                lineLen = 0;
                handleLine(lineBuf);
            }
        } else if ((c == '\b' || c == 0x7F) && lineLen > 0) {
            // Backspace: drop last char and erase it on the terminal
            lineLen--;
            SerialVCP.print("\b \b");
        } else if (lineLen < sizeof(lineBuf) - 1 && c >= ' ') {
            // Printable char: store (if buffer not full) and echo
            lineBuf[lineLen++] = c;
            SerialVCP.write(c);
        }
    }

    static uint32_t last = 0;
    if (cfg.led && millis() - last >= cfg.blink) {
        last = millis();
        digitalToggle(LED_PIN);
    }
}
