// ============================================================================
// BME280: адреси регістрів + перерахунок "сирих" чисел у °C, hPa, %.
// Файл ІДЕНТИЧНИЙ в обох проєктах (ESP32 і STM32): змінюєте тут - копіюйте в інший.
//
// Тут НЕМАЄ нічого про шину (I2C/SPI) і про DMA. Сюди приходять уже прочитані
// байти, тож цей код однаковий для всіх демо - і можна зосередитися на DMA.
// ============================================================================
//
// ЯК ПРАЦЮЄ BME280 (коротко)
//   Усередині датчика - набір 8-бітних регістрів (маленька пам'ять з адресами).
//   - Пишемо налаштування в 0xF2, 0xF5, 0xF4.
//   - Читаємо результат з 0xF7..0xFE (8 байтів підряд одним "пакетом").
//   - "Сирі" числа АЦП треба перерахувати формулами з datasheet, використовуючи
//     калібрувальні коефіцієнти, які виробник записав у кожен чип на заводі
//     (0x88..0xA1 і 0xE1..0xE7). Їх читаємо один раз при старті.
//
// ТОЙ САМИЙ ЧИП - ДВА ІНТЕРФЕЙСИ
//   I2C: адреса пристрою 0x76 (SDO -> GND) або 0x77 (SDO -> 3V3).
//   SPI: адреси пристрою немає (є лінія CS). Старший біт першого байта = напрям:
//        читання  -> reg | 0x80,  запис -> reg & 0x7F.
//
// УВАГА: часто продають BMP280 під виглядом BME280. У BMP280 ID = 0x58 і
// немає датчика вологості (вологість буде нісенітницею).
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint8_t BME280_I2C_ADDR = 0x76;  // 7-бітна адреса (SDO -> GND)

constexpr uint8_t BME280_REG_CALIB1 = 0x88;  // 0x88..0xA1: T1..T3, P1..P9, (0xA0 не використ.), H1
constexpr size_t BME280_CALIB1_LEN = 26;
constexpr uint8_t BME280_REG_ID = 0xD0;
constexpr uint8_t BME280_REG_RESET = 0xE0;
constexpr uint8_t BME280_REG_CALIB2 = 0xE1;  // 0xE1..0xE7: H2..H6
constexpr size_t BME280_CALIB2_LEN = 7;
constexpr uint8_t BME280_REG_CTRL_HUM = 0xF2;
constexpr uint8_t BME280_REG_CTRL_MEAS = 0xF4;
constexpr uint8_t BME280_REG_CONFIG = 0xF5;
constexpr uint8_t BME280_REG_DATA = 0xF7;  // 0xF7..0xFE: тиск[3], температура[3], вологість[2]
constexpr size_t BME280_DATA_LEN = 8;

constexpr uint8_t BME280_CHIP_ID = 0x60;
constexpr uint8_t BMP280_CHIP_ID = 0x58;
constexpr uint8_t BME280_RESET_CMD = 0xB6;

// Режим "normal": датчик сам міряє безперервно, а ми лише читаємо останній
// результат коли захочемо. Порядок запису: CTRL_HUM -> CONFIG -> CTRL_MEAS
// (CTRL_HUM застосовується лише після запису CTRL_MEAS).
constexpr uint8_t BME280_CTRL_HUM_VAL = 0x01;                                 // вологість: oversampling x1
constexpr uint8_t BME280_CONFIG_VAL = (0b011 << 5);                           // пауза між вимірами 250 мс, фільтр вимкнено
constexpr uint8_t BME280_CTRL_MEAS_VAL = (0b001 << 5) | (0b001 << 2) | 0b11;  // T x1, P x1, mode = normal

struct Bme280Calib {
    uint16_t T1;
    int16_t T2, T3;
    uint16_t P1;
    int16_t P2, P3, P4, P5, P6, P7, P8, P9;
    uint8_t H1;
    int16_t H2;
    uint8_t H3;
    int16_t H4, H5;
    int8_t H6;
};

struct Bme280Result {
    float tempC;     // °C
    float pressHpa;  // гПа (hPa)
    float humPct;    // % відносної вологості
};

// Два байти "молодший першим" (little-endian) -> 16-бітне число
inline uint16_t bme280Le16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

// c1 - 26 байтів, прочитаних з 0x88; c2 - 7 байтів, прочитаних з 0xE1.
inline void bme280ParseCalib(const uint8_t* c1, const uint8_t* c2, Bme280Calib& cal)
{
    cal.T1 = bme280Le16(&c1[0]);
    cal.T2 = (int16_t)bme280Le16(&c1[2]);
    cal.T3 = (int16_t)bme280Le16(&c1[4]);
    cal.P1 = bme280Le16(&c1[6]);
    cal.P2 = (int16_t)bme280Le16(&c1[8]);
    cal.P3 = (int16_t)bme280Le16(&c1[10]);
    cal.P4 = (int16_t)bme280Le16(&c1[12]);
    cal.P5 = (int16_t)bme280Le16(&c1[14]);
    cal.P6 = (int16_t)bme280Le16(&c1[16]);
    cal.P7 = (int16_t)bme280Le16(&c1[18]);
    cal.P8 = (int16_t)bme280Le16(&c1[20]);
    cal.P9 = (int16_t)bme280Le16(&c1[22]);
    cal.H1 = c1[25];  // регістр 0xA1

    cal.H2 = (int16_t)bme280Le16(&c2[0]);
    cal.H3 = c2[2];
    // H4 і H5 - 12-бітні числа, "розрізані" між трьома регістрами 0xE4..0xE6
    cal.H4 = (int16_t)(((int8_t)c2[3] * 16) | (c2[4] & 0x0F));
    cal.H5 = (int16_t)(((int8_t)c2[5] * 16) | (c2[4] >> 4));
    cal.H6 = (int8_t)c2[6];
}

// d - 8 байтів, прочитаних з 0xF7. Формули - з datasheet Bosch (розділ 4.2.3),
// цілочисельний варіант. Розбиратися в них не обов'язково: це "магія виробника".
inline void bme280Compensate(const Bme280Calib& cal, const uint8_t* d, Bme280Result& out)
{
    // Сирі значення АЦП: тиск і температура по 20 біт, вологість 16 біт
    const int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    const int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    const int32_t adcH = ((int32_t)d[6] << 8) | d[7];

    // --- температура (рахується першою: t_fine потрібен для тиску і вологості)
    int32_t var1 = (((adcT >> 3) - ((int32_t)cal.T1 << 1)) * (int32_t)cal.T2) >> 11;
    const int32_t dT = (adcT >> 4) - (int32_t)cal.T1;
    int32_t var2 = (((dT * dT) >> 12) * (int32_t)cal.T3) >> 14;
    const int32_t tFine = var1 + var2;
    out.tempC = ((tFine * 5 + 128) >> 8) / 100.0f;

    // --- тиск (64-бітна арифметика), результат у Па * 256
    int64_t p1 = (int64_t)tFine - 128000;
    int64_t p2 = p1 * p1 * cal.P6;
    p2 += (p1 * cal.P5) << 17;
    p2 += (int64_t)cal.P4 << 35;
    p1 = ((p1 * p1 * cal.P3) >> 8) + ((p1 * cal.P2) << 12);
    p1 = ((((int64_t)1 << 47) + p1) * cal.P1) >> 33;
    if (p1 == 0) {
        out.pressHpa = 0;  // захист від ділення на нуль
    }
    else {
        int64_t p = 1048576 - adcP;
        p = (((p << 31) - p2) * 3125) / p1;
        const int64_t v1 = ((int64_t)cal.P9 * (p >> 13) * (p >> 13)) >> 25;
        const int64_t v2 = ((int64_t)cal.P8 * p) >> 19;
        p = ((p + v1 + v2) >> 8) + ((int64_t)cal.P7 << 4);
        out.pressHpa = (float)p / 256.0f / 100.0f;
    }

    // --- вологість, результат у % * 1024
    int32_t v = tFine - 76800;
    v = (((((adcH << 14) - ((int32_t)cal.H4 << 20) - ((int32_t)cal.H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)cal.H6) >> 10) * (((v * (int32_t)cal.H3) >> 11) + 32768)) >> 10) + 2097152) * (int32_t)cal.H2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)cal.H1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    out.humPct = (float)(v >> 12) / 1024.0f;
}
