// ============================================================================
// DMA ДЕМО 4: SPI + BME280                (STM32F411 Black Pill)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ (потрібен 6-контактний модуль BME280 з виводами CSB і SDO;
// 4-контактні модулі вміють лише I2C)
//   VCC -> 3V3,  GND -> GND
//   SCL (=SCK)  -> PA5
//   SDA (=MOSI) -> PA7
//   SDO (=MISO) -> PA6
//   CSB (=CS)   -> PA4
//   Вивід - Serial monitor через USB-C (115200).
//
// ЩО РОБИТЬ ПРОГРАМА (раз на секунду)
//   Читає 8 байтів результату вимірювання двома способами:
//     A) HAL_SPI_TransmitReceive     - без DMA: CPU сам пише/читає кожен байт
//     B) HAL_SPI_TransmitReceive_DMA - байти переносять ДВА потоки DMA, CPU вільний
//
// ЧОМУ ДВА ПОТОКИ DMA?
//   SPI завжди повнодуплексний: щоб ПРИЙНЯТИ байт, master мусить у той самий
//   момент ВІДПРАВИТИ байт (тактуючи лінію SCK). Тому:
//     DMA TX: txBuf -> SPI  (перший байт = номер регістра | 0x80, далі нулі)
//     DMA RX: SPI -> rxBuf  (перший байт - сміття, далі - 8 байтів даних)
//
// ЛІНІЯ CS
//   CS керуємо вручну (звичайний GPIO): опускаємо перед передачею і піднімаємо
//   в callback-у, коли DMA закінчив. Перший спад CS після ввімкнення живлення
//   перемикає BME280 у режим SPI (до наступного вимкнення живлення).
//
// РЕСУРСИ STM32F411: SPI1_RX = DMA2 Stream2 Ch3,  SPI1_TX = DMA2 Stream3 Ch3.
// SPI1 тактується від APB2 = 96 МГц; дільник 64 -> 1.5 МГц (BME280 вміє до 10 МГц).
// ============================================================================
#include <Arduino.h>

#include "bme280_calc.h"

constexpr uint32_t PIN_CS = PA4;

static SPI_HandleTypeDef hspi1;
static DMA_HandleTypeDef hdmaRx;
static DMA_HandleTypeDef hdmaTx;

static uint8_t txBuf[1 + BME280_DATA_LEN];
static uint8_t rxBuf[1 + BME280_DATA_LEN];

static volatile bool spiDone = false;
static volatile bool spiError = false;

static Bme280Calib calib;
static bool isBme = true;

// ---- Обробники переривань -> HAL
extern "C" void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaRx);
}

extern "C" void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaTx);
}

extern "C" void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);  // HAL вмикає його для помилок SPI
}

static void csLow()
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}

static void csHigh()
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

// ---- Callback-и HAL: викликаються з переривання DMA
extern "C" void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* h)
{
    if (h->Instance == SPI1) {
        csHigh();  // транзакцію завершено - відпускаємо датчик
        spiDone = true;
    }
}

extern "C" void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* h)
{
    if (h->Instance == SPI1) {
        csHigh();
        spiError = true;
    }
}

static void dmaStreamInit(DMA_HandleTypeDef& d, DMA_Stream_TypeDef* stream, uint32_t direction)
{
    d.Instance = stream;
    d.Init.Channel = DMA_CHANNEL_3;
    d.Init.Direction = direction;
    d.Init.PeriphInc = DMA_PINC_DISABLE;  // регістр даних SPI один і той самий
    d.Init.MemInc = DMA_MINC_ENABLE;      // по буферу йдемо вперед
    d.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    d.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    d.Init.Mode = DMA_NORMAL;
    d.Init.Priority = DMA_PRIORITY_HIGH;
    d.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&d);
}

static void spiInit()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    // PA5 SCK, PA6 MISO, PA7 MOSI: альтернативна функція AF5 = SPI1
    GPIO_InitTypeDef g = {};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    // PA4 CS: звичайний вихід, у спокої HIGH (датчик не вибраний)
    csHigh();
    g = {};
    g.Pin = GPIO_PIN_4;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    dmaStreamInit(hdmaRx, DMA2_Stream2, DMA_PERIPH_TO_MEMORY);
    dmaStreamInit(hdmaTx, DMA2_Stream3, DMA_MEMORY_TO_PERIPH);
    __HAL_LINKDMA(&hspi1, hdmarx, hdmaRx);
    __HAL_LINKDMA(&hspi1, hdmatx, hdmaTx);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;  // SPI mode 0 (BME280 вміє mode 0 і 3)
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;  // CS керуємо самі
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);

    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

// Блокуюче читання n (<= 26) регістрів, починаючи з reg (без DMA)
static bool readRegs(uint8_t reg, uint8_t* out, uint16_t n)
{
    uint8_t tx[1 + BME280_CALIB1_LEN] = {};
    uint8_t rx[1 + BME280_CALIB1_LEN];
    tx[0] = reg | 0x80;  // старший біт 1 = читання
    csLow();
    const bool ok = HAL_SPI_TransmitReceive(&hspi1, tx, rx, n + 1, 100) == HAL_OK;
    csHigh();
    memcpy(out, &rx[1], n);  // rx[0] прийшов, поки ми передавали номер регістра - це сміття
    return ok;
}

static bool writeReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {(uint8_t)(reg & 0x7F), value};  // старший біт 0 = запис
    csLow();
    const bool ok = HAL_SPI_Transmit(&hspi1, tx, 2, 100) == HAL_OK;
    csHigh();
    return ok;
}

static bool bmeInit()
{
    // "Порожній" спад CS: перемикає BME280 у режим SPI
    csLow();
    delayMicroseconds(10);
    csHigh();

    uint8_t id = 0;
    readRegs(BME280_REG_ID, &id, 1);
    if (id != BME280_CHIP_ID && id != BMP280_CHIP_ID) {
        Serial.printf("Unknown chip ID 0x%02X (0x00/0xFF = no answer: check wiring, 6-pin module?)\n", id);
        return false;
    }
    isBme = id == BME280_CHIP_ID;
    Serial.printf("Found %s (ID 0x%02X)\n", isBme ? "BME280" : "BMP280 (no humidity!)", id);

    writeReg(BME280_REG_RESET, BME280_RESET_CMD);
    delay(10);

    uint8_t c1[BME280_CALIB1_LEN];
    uint8_t c2[BME280_CALIB2_LEN];
    readRegs(BME280_REG_CALIB1, c1, sizeof(c1));
    readRegs(BME280_REG_CALIB2, c2, sizeof(c2));
    bme280ParseCalib(c1, c2, calib);

    return writeReg(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VAL) && writeReg(BME280_REG_CONFIG, BME280_CONFIG_VAL) && writeReg(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VAL);
}

static void printMeasurement(const uint8_t* raw)
{
    Bme280Result r;
    bme280Compensate(calib, raw, r);
    Serial.print("   T = ");
    Serial.print(r.tempC, 2);
    Serial.print(" C   P = ");
    Serial.print(r.pressHpa, 2);
    Serial.print(" hPa");
    if (isBme) {
        Serial.print("   H = ");
        Serial.print(r.humPct, 1);
        Serial.print(" %");
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
    }
    Serial.println("\nDMA demo 4: SPI + BME280 (SPI1: PA4 CS, PA5 SCK, PA6 MISO, PA7 MOSI)");
    spiInit();
    while (!bmeInit()) {
        delay(2000);
    }
}

void loop()
{
    uint8_t raw[BME280_DATA_LEN];
    Serial.println("\n----- read 8 bytes from register 0xF7 -----");

    // ---- A) Без DMA
    uint32_t t0 = micros();
    readRegs(BME280_REG_DATA, raw, BME280_DATA_LEN);
    uint32_t busy = micros() - t0;
    Serial.printf("A) no DMA : %3lu us, CPU busy %3lu us (100%%)\n", (unsigned long)busy, (unsigned long)busy);

    // ---- B) З DMA: готуємо буфер "що відправити", опускаємо CS і запускаємо DMA
    memset(txBuf, 0, sizeof(txBuf));
    memset(rxBuf, 0, sizeof(rxBuf));
    txBuf[0] = BME280_REG_DATA | 0x80;
    spiDone = false;
    spiError = false;
    uint32_t work = 0;

    t0 = micros();
    csLow();
    const bool started = HAL_SPI_TransmitReceive_DMA(&hspi1, txBuf, rxBuf, sizeof(txBuf)) == HAL_OK;
    busy = micros() - t0;
    while (started && !spiDone && !spiError && micros() - t0 < 10000) {
        work++;  // CPU вільний, поки DMA жене байти через SPI
    }
    const uint32_t total = micros() - t0;

    if (!started || !spiDone) {
        csHigh();
        Serial.println("B) DMA    : SPI ERROR");
    }
    else {
        Serial.printf("B) DMA    : %3lu us, CPU busy %3lu us (%3lu%%), other work done: %lu loops\n", (unsigned long)total, (unsigned long)busy, (unsigned long)(busy * 100 / total), (unsigned long)work);
        printMeasurement(&rxBuf[1]);  // rxBuf[0] - сміття (див. readRegs)
    }

    delay(1000);
}
