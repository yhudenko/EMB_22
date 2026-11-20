// STM32F411 (Black Pill) = SPI SLAVE (SPI1, HAL, переривання) + SD карта на SPI2 (master).
//
// Лінія до ESP32 (SPI1, AF5):
//   PA4 NSS <- ESP32 GPIO10, PA5 SCK <- GPIO12, PA6 MISO -> GPIO13, PA7 MOSI <- GPIO11, GND-GND
// HW-125 (SPI2):
//   PB12 -> CS, PB13 -> SCK, PB15 -> MOSI, PB14 <- MISO, VCC -> 5V (модуль має стабілізатор
//   3.3V і перетворювач рівнів), GND -> GND
// Серійний лог іде через USB CDC (Black Pill USB-C).
#include <Arduino.h>
#include <SdFat.h>

#include "spi_proto.h"

// --- SD-карта: STM32 тут MASTER на окремій шині SPI2 ---
SPIClass sdSpi(PB15, PB14, PB13);  // MOSI, MISO, SCK (SPI2)
SdFat sd;
FsFile file;

// --- Slave-інтерфейс до ESP32: SPI1 через HAL ---
SPI_HandleTypeDef hspi1;
static uint8_t rxBuf[FRAME_SIZE];  // сюди HAL складає прийнятий кадр
static uint8_t txBuf[FRAME_SIZE];  // звідси HAL бере байти відповіді (статус)

// Прапорці виставляються в перериванні, а обробляються в loop().
// Довгі операції (SD, Serial) в перериванні робити не можна, тому volatile + прапорець.
static volatile bool frameReady = false;  // повний кадр прийнято
static volatile bool spiError = false;    // помилка SPI (напр. overrun)

// Стан, який ми повідомляємо master через статус-кадр.
static uint8_t lastSeq = 0xFF;
static uint8_t lastStatus = ST_NONE;
static uint8_t badFrames = 0;
static uint32_t totalBytes = 0;

// Вектор переривання SPI1: передаємо керування HAL, який сам читає/пише DR (Data Register)
// і по завершенні 64 байтів викликає HAL_SPI_TxRxCpltCallback.
extern "C" void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}

extern "C" void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* h)
{
    if (h->Instance == SPI1) {
        frameReady = true;
    }
}

extern "C" void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* h)
{
    if (h->Instance == SPI1) {
        spiError = true;
    }
}

// Налаштування SPI1 як slave: піни PA4..PA7 в альтернативну функцію AF5 + параметри SPI.
static void spiSlaveInit()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef g = {};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_4;  // NSS з підтяжкою, щоб не ловити шум поки ESP32 не стартував
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_SLAVE;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;  // mode 0: CPOL=0
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;      //         CPHA=0
    hspi1.Init.NSS = SPI_NSS_HARD_INPUT;  // CS від master апаратно відкриває/закриває транзакцію
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);

    // Дозвіл переривання
    HAL_NVIC_SetPriority(SPI1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

// "Озброює" slave: після виклику SPI готовий прийняти 64 байти в rxBuf і одночасно
// видати txBuf. Якщо master почне транзакцію ДО цього виклику - відповіді не буде.
static void armSpi()
{
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        HAL_SPI_Abort(&hspi1);
    }
    __HAL_SPI_DISABLE(&hspi1);
    __HAL_SPI_CLEAR_OVRFLAG(&hspi1);  // викидає "сміття" з DR, накопичене поки ми були не готові
    HAL_SPI_TransmitReceive_IT(&hspi1, txBuf, rxBuf, FRAME_SIZE);
}

// Збирає в txBuf статус-кадр (відповідь), який піде master у НАСТУПНІЙ транзакції.
static void buildStatus()
{
    memset(txBuf, 0, FRAME_SIZE);
    txBuf[0] = MAGIC_RSP;
    txBuf[1] = lastSeq;
    txBuf[2] = lastStatus;
    txBuf[3] = badFrames;
    memcpy(&txBuf[4], &totalBytes, 4);  // little-endian
    txBuf[FRAME_SIZE - 1] = crc8(txBuf, FRAME_SIZE - 1);
}

// Обробка прийнятого кадру: перевірка -> запис на SD -> оновлення статусу.
static void processFrame()
{
    const bool valid = rxBuf[0] == MAGIC_REQ && crc8(rxBuf, FRAME_SIZE - 1) == rxBuf[FRAME_SIZE - 1];
    if (!valid) {
        badFrames++;  // статус не чіпаємо: master побачить, що seq не підтверджено
        Serial.println("bad frame (magic/crc)");
        return;
    }
    if (rxBuf[1] != TYPE_DATA) {
        return;  // POLL: просто віддаємо поточний статус
    }

    const uint8_t seq = rxBuf[2];
    const uint8_t len = rxBuf[3];
    if (len > PAYLOAD_MAX) {
        badFrames++;
        return;
    }
    if (seq == lastSeq && lastStatus == ST_OK) {
        return;  // повторна відправка вже записаного кадру
    }

    // write() кладе в буфер, sync() реально скидає на карту (повільно, але дані не загубляться при відключенні)
    const bool ok = file.write(&rxBuf[4], len) == len && file.sync();
    lastSeq = seq;
    lastStatus = ok ? ST_OK : ST_SD_ERR;
    if (ok) {
        totalBytes += len;
    }
    digitalToggle(PC13);
    Serial.printf("seq=%u len=%u %s\n", seq, len, ok ? "written" : "SD ERROR");
}

void setup()
{
    Serial.begin(115200);
    pinMode(PC13, OUTPUT);
    digitalWrite(PC13, HIGH);

    while (!sd.begin(SdSpiConfig(PB12, DEDICATED_SPI, SD_SCK_MHZ(8), &sdSpi))) {
        Serial.println("SD init failed, retry");
        digitalToggle(PC13);
        delay(500);
    }
    if (!file.open("data.csv", O_WRONLY | O_CREAT | O_APPEND)) {
        Serial.println("open data.csv failed");
        while (true) {
            digitalToggle(PC13);
            delay(100);
        }
    }

    spiSlaveInit();
    buildStatus();
    armSpi();
    Serial.println("STM32 SPI slave ready");
}

void loop()
{
    static uint32_t partialSince = 0;

    // Основний цикл: кадр прийнято (перериванням) -> обробити -> підготувати статус -> знову озброїти SPI.
    if (frameReady) {
        frameReady = false;
        processFrame();
        buildStatus();  // оновлюємо відповідь ДО повторного "озброєння"
        armSpi();
    }
    else if (spiError) {
        spiError = false;
        armSpi();
    }

    // Захист від розсинхронізації: прийнято частину кадру, а CS вже відпущено (напр. ESP32 перезавантажився).
    const bool partial = hspi1.RxXferCount > 0 && hspi1.RxXferCount < FRAME_SIZE;
    if (partial && (GPIOA->IDR & GPIO_PIN_4)) {
        if (!partialSince) {
            partialSince = millis();
        }
        if (millis() - partialSince > 5) {
            partialSince = 0;
            armSpi();
        }
    }
    else {
        partialSince = 0;
    }
}
