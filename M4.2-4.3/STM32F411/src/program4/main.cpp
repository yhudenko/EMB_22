// Education example: BME280 over I2C1 using STM32 HAL (Black Pill F411).
//
// Wiring:
//   BME280 SDA -> PB7 (I2C1_SDA, AF4)
//   BME280 SCL -> PB6 (I2C1_SCL, AF4)
//   VCC -> 3V3, GND -> GND, SDO -> GND (address 0x76) or 3V3 (0x77)
// Serial output (USART1): TX = PA9, 115200 8N1. Clock: default 16 MHz HSI.

#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

static I2C_HandleTypeDef hi2c1;
static UART_HandleTypeDef huart1;

constexpr uint16_t BME280_ADDR = 0x76 << 1;  // HAL wants 8-bit (shifted) address

// Calibration values (datasheet naming)
static uint16_t dig_T1, dig_P1;
static int16_t dig_T2, dig_T3;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t dig_H1, dig_H3;
static int16_t dig_H2, dig_H4, dig_H5;
static int8_t dig_H6;
static int32_t t_fine;

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
}

// printf -> USART1
extern "C" int _write(int, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

static void error_loop()
{
    while (true) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  // blink LED on error
        HAL_Delay(200);
    }
}

static void gpio_init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {};

    // Onboard LED PC13
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &g);

    // I2C1 pins: open-drain, AF4
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    // USART1 TX: PA9, AF7
    g.Pin = GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);
}

static void peripherals_init()
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) error_loop();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) error_loop();
}

static HAL_StatusTypeDef bme_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static HAL_StatusTypeDef bme_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static bool bme_init()
{
    uint8_t id = 0;
    if (bme_read(0xD0, &id, 1) != HAL_OK || id != 0x60) return false;  // chip id

    bme_write(0xE0, 0xB6);  // soft reset
    HAL_Delay(10);

    // Read calibration data
    uint8_t c[26], h[7];
    if (bme_read(0x88, c, 26) != HAL_OK) return false;  // 0x88..0xA1
    if (bme_read(0xE1, h, 7) != HAL_OK) return false;   // 0xE1..0xE7

    dig_T1 = c[0] | (c[1] << 8);
    dig_T2 = (int16_t)(c[2] | (c[3] << 8));
    dig_T3 = (int16_t)(c[4] | (c[5] << 8));
    dig_P1 = c[6] | (c[7] << 8);
    dig_P2 = (int16_t)(c[8] | (c[9] << 8));
    dig_P3 = (int16_t)(c[10] | (c[11] << 8));
    dig_P4 = (int16_t)(c[12] | (c[13] << 8));
    dig_P5 = (int16_t)(c[14] | (c[15] << 8));
    dig_P6 = (int16_t)(c[16] | (c[17] << 8));
    dig_P7 = (int16_t)(c[18] | (c[19] << 8));
    dig_P8 = (int16_t)(c[20] | (c[21] << 8));
    dig_P9 = (int16_t)(c[22] | (c[23] << 8));
    dig_H1 = c[25];
    dig_H2 = (int16_t)(h[0] | (h[1] << 8));
    dig_H3 = h[2];
    dig_H4 = (int16_t)((h[3] << 4) | (h[4] & 0x0F));
    dig_H5 = (int16_t)((h[5] << 4) | (h[4] >> 4));
    dig_H6 = (int8_t)h[6];

    // Humidity x1 (must be written before ctrl_meas), temp x1, press x1, normal mode
    bme_write(0xF2, 0x01);
    bme_write(0xF4, (1 << 5) | (1 << 2) | 0x03);
    return true;
}

// Compensation formulas: 32-bit integer versions from the BME280 datasheet
static int32_t comp_temp(int32_t adc_T)  // returns 0.01 degC
{
    int32_t v1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * (int32_t)dig_T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - (int32_t)dig_T1) * ((adc_T >> 4) - (int32_t)dig_T1)) >> 12) *
                  (int32_t)dig_T3) >> 14;
    t_fine = v1 + v2;
    return (t_fine * 5 + 128) >> 8;
}

static uint32_t comp_press(int32_t adc_P)  // returns Pa
{
    int32_t v1 = (t_fine >> 1) - 64000;
    int32_t v2 = (((v1 >> 2) * (v1 >> 2)) >> 11) * (int32_t)dig_P6;
    v2 = v2 + ((v1 * (int32_t)dig_P5) << 1);
    v2 = (v2 >> 2) + ((int32_t)dig_P4 << 16);
    v1 = (((dig_P3 * (((v1 >> 2) * (v1 >> 2)) >> 13)) >> 3) + (((int32_t)dig_P2 * v1) >> 1)) >> 18;
    v1 = ((32768 + v1) * (int32_t)dig_P1) >> 15;
    if (v1 == 0) return 0;
    uint32_t p = (((uint32_t)(((int32_t)1048576) - adc_P) - (v2 >> 12))) * 3125;
    if (p < 0x80000000) p = (p << 1) / (uint32_t)v1;
    else p = (p / (uint32_t)v1) * 2;
    v1 = ((int32_t)dig_P9 * (int32_t)(((p >> 3) * (p >> 3)) >> 13)) >> 12;
    v2 = ((int32_t)(p >> 2) * (int32_t)dig_P8) >> 13;
    return (uint32_t)((int32_t)p + ((v1 + v2 + dig_P7) >> 4));
}

static uint32_t comp_hum(int32_t adc_H)  // returns %RH * 1024
{
    int32_t v = t_fine - 76800;
    v = (((((adc_H << 14) - ((int32_t)dig_H4 << 20) - ((int32_t)dig_H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)dig_H6) >> 10) * (((v * (int32_t)dig_H3) >> 11) + 32768)) >> 10) +
            2097152) * (int32_t)dig_H2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return (uint32_t)(v >> 12);
}

int main()
{
    HAL_Init();
    gpio_init();
    peripherals_init();

    printf("BME280 HAL example\r\n");
    if (!bme_init()) {
        printf("BME280 not found\r\n");
        error_loop();
    }

    while (true) {
        uint8_t d[8];
        if (bme_read(0xF7, d, 8) == HAL_OK) {  // burst read: press, temp, hum
            int32_t adc_P = ((int32_t)d[0] << 12) | (d[1] << 4) | (d[2] >> 4);
            int32_t adc_T = ((int32_t)d[3] << 12) | (d[4] << 4) | (d[5] >> 4);
            int32_t adc_H = ((int32_t)d[6] << 8) | d[7];

            int32_t t = comp_temp(adc_T);  // temp first: sets t_fine
            uint32_t p = comp_press(adc_P);
            uint32_t h = comp_hum(adc_H);

            // Integer printing, so no float printf support is needed
            printf("T=%ld.%02ld C  P=%lu.%02lu hPa  H=%lu.%02lu %%\r\n",
                   (long)(t / 100), (long)(t % 100),
                   (unsigned long)(p / 100), (unsigned long)(p % 100),
                   (unsigned long)(h / 1024), (unsigned long)((h % 1024) * 100 / 1024));
        } else {
            printf("I2C read error\r\n");
        }
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(1000);
    }
}
