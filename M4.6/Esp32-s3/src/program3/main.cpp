// ============================================================================
// ДЕМО 3: I2C + BME280                    (ESP32-S3)
// ============================================================================
//
// ВАЖЛИВО: У ESP32-S3 I2C НЕ МАЄ DMA!
//   GDMA під'єднується лише до SPI, I2S, LCD/CAM, ADC, UHCI(UART), AES, SHA
//   (Technical Reference Manual, розділ GDMA). Не кожна периферія вміє DMA -
//   це завжди треба перевіряти в документації конкретного чипа.
//   (Для порівняння: у STM32F411 I2C з DMA є - див. STM32 program3.)
//
// ЯК ТОДІ ESP32 ЗВІЛЬНЯЄ CPU ПІД ЧАС I2C?
//   Апаратний I2C у ESP32 має:
//     - "список команд" (до 8 кроків: START, WRITE, READ, STOP ...), який
//       контролер виконує САМ, без CPU;
//     - FIFO на 32 байти для даних.
//   CPU записує команди + дані, запускає - і задача "засинає" (FreeRTOS).
//   Коли все виконано, переривання "будить" задачу. Поки вона спить, CPU
//   виконує ІНШІ задачі. Для коротких передач (<= 32 байти) це майже як DMA.
//   Arduino Wire на ESP32 працює саме так (через драйвер ESP-IDF).
//
// ЩО РОБИТЬ ПРОГРАМА (раз на секунду)
//   Паралельно працює фонова задача "інша робота" - вона просто рахує, але
//   отримує CPU лише тоді, коли головна задача (loop) чекає.
//     A) Читаємо 8 байтів BME280 через Wire -> дивимось, скільки фонова задача
//        встигла нарахувати = скільки CPU був вільний.
//     B) Для порівняння: стільки ж часу CPU крутиться в циклі (як "програмний
//        I2C", де CPU сам смикає ніжки) -> фонова задача не отримує нічого.
//
// ПІДКЛЮЧЕННЯ (модуль BME280)
//   VCC -> 3V3, GND -> GND, SDA -> GPIO8, SCL -> GPIO9
//   SDO -> GND (адреса 0x76; якщо SDO -> 3V3, змініть BME280_I2C_ADDR на 0x77)
//   Вивід - Serial0 (115200).
// ============================================================================
#include <Arduino.h>
#include <Wire.h>

#include "bme280_calc.h"

constexpr int PIN_SDA = 8;
constexpr int PIN_SCL = 9;
constexpr uint32_t I2C_HZ = 100000;  // 100 кГц: 1 байт ~ 90 мкс, усе читання ~1 мс

static Bme280Calib calib;
static bool isBme = true;

// Лічильник "іншої роботи". Його збільшує фонова задача.
static volatile uint32_t freeWork = 0;
static uint32_t workPerMs = 1;  // скільки фонова задача рахує за 1 мс, коли CPU повністю вільний

// Фонова задача з НАЙНИЖЧИМ пріоритетом (0): FreeRTOS дає їй CPU лише тоді,
// коли всі важливіші задачі (наш loop має пріоритет 1) чекають.
static void freeWorkTask(void*)
{
    for (;;) {
        freeWork++;
    }
}

static bool readRegs(uint8_t reg, uint8_t* buf, size_t n)
{
    Wire.beginTransmission(BME280_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {  // false = без STOP, далі буде RESTART
        return false;
    }
    if (Wire.requestFrom((int)BME280_I2C_ADDR, (int)n) != (int)n) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

static bool writeReg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(BME280_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool bmeInit()
{
    uint8_t id = 0;
    if (!readRegs(BME280_REG_ID, &id, 1)) {
        Serial0.printf("No answer at I2C address 0x%02X. Check wiring / SDO pin.\n", BME280_I2C_ADDR);
        return false;
    }
    if (id != BME280_CHIP_ID && id != BMP280_CHIP_ID) {
        Serial0.printf("Unknown chip ID 0x%02X\n", id);
        return false;
    }
    isBme = id == BME280_CHIP_ID;
    Serial0.printf("Found %s (ID 0x%02X)\n", isBme ? "BME280" : "BMP280 (no humidity!)", id);

    writeReg(BME280_REG_RESET, BME280_RESET_CMD);
    delay(10);

    uint8_t c1[BME280_CALIB1_LEN];
    uint8_t c2[BME280_CALIB2_LEN];
    if (!readRegs(BME280_REG_CALIB1, c1, sizeof(c1)) || !readRegs(BME280_REG_CALIB2, c2, sizeof(c2))) {
        return false;
    }
    bme280ParseCalib(c1, c2, calib);

    return writeReg(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VAL) && writeReg(BME280_REG_CONFIG, BME280_CONFIG_VAL) && writeReg(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VAL);
}

// Скільки % часу CPU був вільний: що фонова задача встигла / що встигла б за той самий час
static uint32_t freePercent(uint32_t work, uint32_t us)
{
    const uint32_t expected = (uint32_t)((uint64_t)workPerMs * us / 1000);
    const uint32_t pct = expected ? (uint32_t)((uint64_t)work * 100 / expected) : 0;
    return pct > 100 ? 100 : pct;
}

void setup()
{
    Serial0.begin(115200);
    delay(500);
    Serial0.println("\nDemo 3: I2C + BME280 on ESP32-S3 (no DMA for I2C on this chip!)");

    Wire.begin(PIN_SDA, PIN_SCL, I2C_HZ);
    while (!bmeInit()) {
        delay(2000);
    }

    // Фонова задача на тому ж ядрі, що й loop(), з пріоритетом 0
    xTaskCreatePinnedToCore(freeWorkTask, "freeWork", 2048, nullptr, 0, nullptr, xPortGetCoreID());

    // Калібровка: поки loop "спить" 100 мс, увесь CPU дістається фоновій задачі
    const uint32_t c0 = freeWork;
    delay(100);
    workPerMs = (freeWork - c0) / 100;
    Serial0.printf("Background task does %lu loops/ms when the CPU is completely free\n", (unsigned long)workPerMs);
}

void loop()
{
    uint8_t raw[BME280_DATA_LEN];
    Serial0.println("\n----- read 8 bytes from register 0xF7 -----");

    // ---- A) Апаратний I2C: задача чекає, CPU віддається іншим задачам
    uint32_t c0 = freeWork;
    uint32_t t0 = micros();
    const bool ok = readRegs(BME280_REG_DATA, raw, BME280_DATA_LEN);
    const uint32_t i2cUs = micros() - t0;
    uint32_t work = freeWork - c0;
    Serial0.printf("A) hardware I2C (Wire)       : %4lu us, CPU free %3lu%% (other task did %lu loops)\n", (unsigned long)i2cUs, (unsigned long)freePercent(work, i2cUs), (unsigned long)work);

    // ---- B) CPU "зайнятий" той самий час (як програмний I2C, bit-banging)
    c0 = freeWork;
    t0 = micros();
    delayMicroseconds(i2cUs);  // активне очікування: CPU крутиться в циклі
    const uint32_t busyUs = micros() - t0;
    work = freeWork - c0;
    Serial0.printf("B) CPU busy-waits same time  : %4lu us, CPU free %3lu%% (other task did %lu loops)\n", (unsigned long)busyUs, (unsigned long)freePercent(work, busyUs), (unsigned long)work);

    if (ok) {
        Bme280Result r;
        bme280Compensate(calib, raw, r);
        if (isBme) {
            Serial0.printf("   T = %.2f C   P = %.2f hPa   H = %.1f %%\n", r.tempC, r.pressHpa, r.humPct);
        }
        else {
            Serial0.printf("   T = %.2f C   P = %.2f hPa\n", r.tempC, r.pressHpa);
        }
    }
    else {
        Serial0.println("   I2C ERROR");
    }

    delay(1000);
}
