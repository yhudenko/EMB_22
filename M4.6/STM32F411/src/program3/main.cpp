// ============================================================================
// DMA ДЕМО 3: I2C + BME280                (STM32F411 Black Pill)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ (модуль BME280)
//   VCC -> 3V3,  GND -> GND,  SCL -> PB6,  SDA -> PB7
//   SDO -> GND (адреса 0x76; якщо SDO -> 3V3, змініть BME280_I2C_ADDR на 0x77)
//   CSB -> 3V3 або не підключати (на модулі вже є підтяжка = режим I2C)
//   Вивід - Serial monitor через USB-C (115200).
//
// ЩО РОБИТЬ ПРОГРАМА (раз на секунду)
//   Читає 8 байтів результату вимірювання двома способами:
//     A) HAL_I2C_Mem_Read     - без DMA: CPU сам забирає кожен байт і чекає
//     B) HAL_I2C_Mem_Read_DMA - байти складає в буфер DMA, CPU вільний
//   і показує, скільки часу CPU був зайнятий у кожному випадку.
//
// ЯК ВИГЛЯДАЄ ЧИТАННЯ РЕГІСТРІВ ПО I2C
//   [START][0x76+W][0xF7][RESTART][0x76+R][байт1]...[байт8][STOP]
//    \______ "адресна фаза" _____/        \____ дані ____/
//   На 100 кГц один байт (8 біт + ACK) = 90 мкс, тобто все читання ~1 мс.
//   Для мікроконтролера на 96 МГц це ~96 000 тактів очікування!
//
// ЧЕСНЕ ЗАУВАЖЕННЯ ПРО HAL НА STM32F4
//   У HAL_I2C_Mem_Read_DMA "адресна фаза" (START, адреса, номер регістра,
//   RESTART) виконується БЕЗ DMA - CPU чекає її ~0.3 мс. DMA переносить
//   лише самі дані. Тому тут CPU вільний не 100%, а ~70% часу. Чим більше
//   байтів читаєте (EEPROM, дисплей), тим більша вигода від DMA.
//   Для 8 байтів DMA - скоріше навчальний приклад, ніж необхідність.
//
// РЕСУРСИ STM32F411: I2C1_RX = DMA1 Stream0 Channel1.
// ============================================================================
#include <Arduino.h>

#include "bme280_calc.h"

constexpr uint16_t DEV = BME280_I2C_ADDR << 1;  // HAL хоче адресу, вже зсунуту на 1 біт вліво

static I2C_HandleTypeDef hi2c1;
static DMA_HandleTypeDef hdmaRx;

static volatile bool rxDone = false;
static volatile bool i2cError = false;

static Bme280Calib calib;
static bool isBme = true;  // false = це BMP280 (без вологості)

// ---- Обробники переривань -> HAL
extern "C" void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaRx);
}

extern "C" void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

extern "C" void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c1);
}

// ---- Callback-и HAL
// Викликається, коли DMA прийняв усі байти (HAL сам формує STOP на шині)
extern "C" void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* h)
{
    if (h->Instance == I2C1) {
        rxDone = true;
    }
}

extern "C" void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* h)
{
    if (h->Instance == I2C1) {
        i2cError = true;
    }
}

static void i2cInit()
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // PB6 = SCL, PB7 = SDA. I2C завжди "відкритий стік" (OD) + підтяжка вгору.
    GPIO_InitTypeDef g = {};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;  // внутрішня ~40 кОм - запасний варіант; на модулі зазвичай є 4.7-10 кОм
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    // DMA: периферія (регістр даних I2C) -> пам'ять (наш буфер)
    hdmaRx.Instance = DMA1_Stream0;
    hdmaRx.Init.Channel = DMA_CHANNEL_1;
    hdmaRx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdmaRx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdmaRx.Init.MemInc = DMA_MINC_ENABLE;
    hdmaRx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdmaRx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdmaRx.Init.Mode = DMA_NORMAL;
    hdmaRx.Init.Priority = DMA_PRIORITY_HIGH;
    hdmaRx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdmaRx);
    __HAL_LINKDMA(&hi2c1, hdmarx, hdmaRx);

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;  // 100 кГц (BME280 вміє і 400 кГц)
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

// Прості блокуючі (без DMA) читання/запис - для налаштування при старті
static bool readRegs(uint8_t reg, uint8_t* buf, uint16_t n)
{
    return HAL_I2C_Mem_Read(&hi2c1, DEV, reg, I2C_MEMADD_SIZE_8BIT, buf, n, 100) == HAL_OK;
}

static bool writeReg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c1, DEV, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100) == HAL_OK;
}

static bool bmeInit()
{
    uint8_t id = 0;
    if (!readRegs(BME280_REG_ID, &id, 1)) {
        Serial.printf("No answer at I2C address 0x%02X. Check wiring / SDO pin.\n", BME280_I2C_ADDR);
        return false;
    }
    if (id != BME280_CHIP_ID && id != BMP280_CHIP_ID) {
        Serial.printf("Unknown chip ID 0x%02X\n", id);
        return false;
    }
    isBme = id == BME280_CHIP_ID;
    Serial.printf("Found %s (ID 0x%02X)\n", isBme ? "BME280" : "BMP280 (no humidity!)", id);

    writeReg(BME280_REG_RESET, BME280_RESET_CMD);
    delay(10);  // після reset датчик копіює калібровку зі своєї пам'яті

    uint8_t c1[BME280_CALIB1_LEN];
    uint8_t c2[BME280_CALIB2_LEN];
    if (!readRegs(BME280_REG_CALIB1, c1, sizeof(c1)) || !readRegs(BME280_REG_CALIB2, c2, sizeof(c2))) {
        return false;
    }
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
    Serial.println("\nDMA demo 3: I2C + BME280 (I2C1: PB6 SCL, PB7 SDA)");
    i2cInit();
    while (!bmeInit()) {
        delay(2000);
    }
}

void loop()
{
    static uint8_t raw[BME280_DATA_LEN];
    Serial.println("\n----- read 8 bytes from register 0xF7 -----");

    // ---- A) Без DMA: функція повертається, коли всі байти вже прийняті
    uint32_t t0 = micros();
    const bool okA = readRegs(BME280_REG_DATA, raw, BME280_DATA_LEN);
    uint32_t busy = micros() - t0;
    Serial.printf("A) no DMA : %4lu us, CPU busy %4lu us (100%%) %s\n", (unsigned long)busy, (unsigned long)busy, okA ? "" : "I2C ERROR");

    // ---- B) З DMA: функція запускає читання і повертається; дані прийдуть самі
    memset(raw, 0, sizeof(raw));
    rxDone = false;
    i2cError = false;
    uint32_t work = 0;
    t0 = micros();
    const bool started = HAL_I2C_Mem_Read_DMA(&hi2c1, DEV, BME280_REG_DATA, I2C_MEMADD_SIZE_8BIT, raw, BME280_DATA_LEN) == HAL_OK;
    busy = micros() - t0;
    while (started && !rxDone && !i2cError && micros() - t0 < 20000) {
        work++;  // CPU робить "інше", поки DMA приймає байти
    }
    const uint32_t total = micros() - t0;

    if (!started || !rxDone) {
        Serial.println("B) DMA    : I2C ERROR");
    }
    else {
        Serial.printf("B) DMA    : %4lu us, CPU busy %4lu us (%3lu%%), other work done: %lu loops\n", (unsigned long)total, (unsigned long)busy, (unsigned long)(busy * 100 / total), (unsigned long)work);
        printMeasurement(raw);
    }

    delay(1000);
}
