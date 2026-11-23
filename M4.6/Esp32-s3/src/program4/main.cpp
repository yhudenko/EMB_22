// ============================================================================
// DMA ДЕМО 4: SPI + BME280                (ESP32-S3)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ (потрібен 6-контактний модуль BME280 з виводами CSB і SDO;
// 4-контактні модулі вміють лише I2C)
//   VCC -> 3V3,  GND -> GND
//   SCL (=SCK)  -> GPIO12
//   SDA (=MOSI) -> GPIO11
//   SDO (=MISO) -> GPIO13
//   CSB (=CS)   -> GPIO10
//   Вивід - Serial0 (115200).
//
// SPI + DMA НА ESP32-S3
//   Тут використовуємо драйвер SPI з ESP-IDF (driver/spi_master.h), а не
//   Arduino SPI.h: Arduino-бібліотека на ESP32 DMA не використовує.
//   spi_bus_initialize(..., SPI_DMA_CH_AUTO) - драйвер сам бере канал GDMA,
//   і далі ВСІ байти між пам'яттю і SPI переносить DMA. CS драйвер теж
//   опускає/піднімає апаратно.
//
// ЩО РОБИТЬ ПРОГРАМА (раз на секунду) - читає 8 байтів двома способами:
//   A) spi_device_polling_transmit: CPU крутиться в циклі, чекаючи кінця
//      передачі ("опитування", polling). Зайнятий 100% часу.
//   B) spi_device_queue_trans: CPU лише ставить транзакцію в чергу і вільний.
//      Кінець передачі повідомляє переривання (наш callback post_cb).
//   В обох випадках байти переносить DMA; різниця - чим CPU зайнятий тим часом.
//
// ЧЕСНЕ ЗАУВАЖЕННЯ
//   9 байтів на 1 МГц - це ~72 мкс, а підготовка транзакції драйвером теж
//   займає десятки мкс. Тому для ТАКИХ коротких передач виграш невеликий.
//   DMA "розкривається" на великих обсягах: дисплей (сотні КБ на кадр),
//   SD-карта, АЦП, аудіо. Без DMA ESP32 за раз передає не більше 64 байтів
//   (розмір буфера SPI), з DMA - до 32 КБ за одну транзакцію.
//
// SPI ЗАВЖДИ ПОВНОДУПЛЕКСНИЙ: щоб прийняти байт, master мусить у той самий момент
// відправити байт. Тому відправляємо [reg | 0x80][0][0]...[0], а приймаємо
// [сміття][8 байтів даних].
// ============================================================================
#include <Arduino.h>

#include "bme280_calc.h"
#include "driver/spi_master.h"

constexpr int PIN_CS = 10;
constexpr int PIN_MOSI = 11;
constexpr int PIN_SCK = 12;
constexpr int PIN_MISO = 13;
constexpr int SPI_HZ = 1000000;  // 1 МГц (BME280 вміє до 10 МГц)

static spi_device_handle_t bme = nullptr;

// DMA_ATTR = буфер у внутрішній RAM, вирівняний на 4 байти (вимога DMA)
DMA_ATTR static uint8_t txBuf[32];
DMA_ATTR static uint8_t rxBuf[32];

static Bme280Calib calib;
static bool isBme = true;
static volatile bool spiDone = false;

// Callback драйвера: викликається з ПЕРЕРИВАННЯ після кожної транзакції
static void IRAM_ATTR onSpiDone(spi_transaction_t*)
{
    spiDone = true;
}

static void spiInit()
{
    spi_bus_config_t bus = {};
    bus.mosi_io_num = PIN_MOSI;
    bus.miso_io_num = PIN_MISO;
    bus.sclk_io_num = PIN_SCK;
    bus.quadwp_io_num = -1;  // не використовуємо (лише для QSPI)
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = sizeof(txBuf);
    // SPI2 ("FSPI") + DMA: канал GDMA обере драйвер
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = SPI_HZ;
    dev.mode = 0;  // CPOL=0, CPHA=0 (BME280 вміє mode 0 і 3)
    dev.spics_io_num = PIN_CS;  // CS керує драйвер
    dev.queue_size = 2;         // скільки транзакцій можна поставити в чергу
    dev.post_cb = onSpiDone;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &bme));
}

// Готує транзакцію "прочитати n регістрів, починаючи з reg"
static void prepareRead(spi_transaction_t& t, uint8_t reg, size_t n)
{
    memset(&t, 0, sizeof(t));
    memset(txBuf, 0, sizeof(txBuf));
    memset(rxBuf, 0, sizeof(rxBuf));
    txBuf[0] = reg | 0x80;     // старший біт 1 = читання
    t.length = (n + 1) * 8;    // довжина в БІТАХ: 1 байт адреси + n байтів даних
    t.tx_buffer = txBuf;
    t.rx_buffer = rxBuf;
}

// Звичайне (блокуюче) читання - для налаштування при старті
static void readRegs(uint8_t reg, uint8_t* out, size_t n)
{
    spi_transaction_t t;
    prepareRead(t, reg, n);
    spi_device_polling_transmit(bme, &t);
    memcpy(out, &rxBuf[1], n);  // rxBuf[0] - сміття, поки передавали номер регістра
}

static void writeReg(uint8_t reg, uint8_t value)
{
    spi_transaction_t t = {};
    txBuf[0] = reg & 0x7F;  // старший біт 0 = запис
    txBuf[1] = value;
    t.length = 16;
    t.tx_buffer = txBuf;
    spi_device_polling_transmit(bme, &t);
}

static bool bmeInit()
{
    uint8_t id = 0;
    readRegs(BME280_REG_ID, &id, 1);  // перший спад CS ще й перемикає BME280 у режим SPI
    readRegs(BME280_REG_ID, &id, 1);
    if (id != BME280_CHIP_ID && id != BMP280_CHIP_ID) {
        Serial0.printf("Unknown chip ID 0x%02X (0x00/0xFF = no answer: check wiring, 6-pin module?)\n", id);
        return false;
    }
    isBme = id == BME280_CHIP_ID;
    Serial0.printf("Found %s (ID 0x%02X)\n", isBme ? "BME280" : "BMP280 (no humidity!)", id);

    writeReg(BME280_REG_RESET, BME280_RESET_CMD);
    delay(10);

    uint8_t c1[BME280_CALIB1_LEN];
    uint8_t c2[BME280_CALIB2_LEN];
    readRegs(BME280_REG_CALIB1, c1, sizeof(c1));
    readRegs(BME280_REG_CALIB2, c2, sizeof(c2));
    bme280ParseCalib(c1, c2, calib);

    writeReg(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VAL);
    writeReg(BME280_REG_CONFIG, BME280_CONFIG_VAL);
    writeReg(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VAL);
    return true;
}

void setup()
{
    Serial0.begin(115200);
    delay(500);
    Serial0.println("\nDMA demo 4: SPI + BME280 (ESP32-S3, SPI2 + GDMA)");
    spiInit();
    while (!bmeInit()) {
        delay(2000);
    }
}

void loop()
{
    spi_transaction_t t;
    Serial0.println("\n----- read 8 bytes from register 0xF7 -----");

    // ---- A) polling: функція повертається лише після кінця передачі
    prepareRead(t, BME280_REG_DATA, BME280_DATA_LEN);
    uint32_t t0 = micros();
    spi_device_polling_transmit(bme, &t);
    uint32_t busy = micros() - t0;
    Serial0.printf("A) polling (CPU waits) : %3lu us, CPU busy %3lu us (100%%)\n", (unsigned long)busy, (unsigned long)busy);

    // ---- B) queue: поставили в чергу - і вільні. Кінець повідомить callback.
    prepareRead(t, BME280_REG_DATA, BME280_DATA_LEN);
    spiDone = false;
    uint32_t work = 0;
    t0 = micros();
    spi_device_queue_trans(bme, &t, portMAX_DELAY);
    busy = micros() - t0;
    while (!spiDone) {
        work++;  // CPU вільний, поки DMA жене байти через SPI
    }
    const uint32_t total = micros() - t0;
    spi_transaction_t* finished;
    spi_device_get_trans_result(bme, &finished, portMAX_DELAY);  // "забрати" завершену транзакцію з черги
    Serial0.printf("B) queued (DMA + IRQ)  : %3lu us, CPU busy %3lu us (%3lu%%), other work done: %lu loops\n", (unsigned long)total, (unsigned long)busy, (unsigned long)(busy * 100 / total), (unsigned long)work);

    Bme280Result r;
    bme280Compensate(calib, &rxBuf[1], r);
    if (isBme) {
        Serial0.printf("   T = %.2f C   P = %.2f hPa   H = %.1f %%\n", r.tempC, r.pressHpa, r.humPct);
    }
    else {
        Serial0.printf("   T = %.2f C   P = %.2f hPa\n", r.tempC, r.pressHpa);
    }

    delay(1000);
}
