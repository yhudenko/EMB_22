// Bare-metal I2C1 master driver + BME280 sensor example for STM32F411 (Black Pill).
// No Arduino / HAL: only CMSIS register definitions.
//
// Wiring:
//   BME280 SDA -> PB7 (I2C1_SDA, AF4)
//   BME280 SCL -> PB6 (I2C1_SCL, AF4)
//   BME280 VCC -> 3V3, GND -> GND
//   SDO -> GND gives address 0x76, SDO -> 3V3 gives address 0x77
//   Most breakout boards already have 4.7k pull-ups; the internal ones (~40k) are enabled as a fallback only.
//
// Serial output (USART1): TX = PA9, RX = PA10, 115200 8N1.
//
// Clock: the chip runs from the default 16 MHz HSI (no PLL) to keep the example short.

#include <stdint.h>
#include <stdio.h>

#include "stm32f4xx.h"

constexpr uint32_t PCLK_HZ = 16000000;  // HCLK = APB1 = APB2 = HSI
constexpr uint32_t I2C_SPEED_HZ = 100000;
constexpr uint32_t I2C_TIMEOUT_MS = 10;
constexpr uint32_t UART_BAUD = 115200;

constexpr uint8_t BME280_ADDR = 0x76;

// BME280 registers
constexpr uint8_t REG_CALIB_00 = 0x88;  // 0x88..0x9F: T1..T3, P1..P9
constexpr uint8_t REG_CALIB_H1 = 0xA1;
constexpr uint8_t REG_ID = 0xD0;
constexpr uint8_t REG_RESET = 0xE0;
constexpr uint8_t REG_CALIB_26 = 0xE1;  // 0xE1..0xE7: H2..H6
constexpr uint8_t REG_CTRL_HUM = 0xF2;
constexpr uint8_t REG_STATUS = 0xF3;
constexpr uint8_t REG_CTRL_MEAS = 0xF4;
constexpr uint8_t REG_CONFIG = 0xF5;
constexpr uint8_t REG_DATA = 0xF7;  // 0xF7..0xFE: press[3], temp[3], hum[2]

constexpr uint8_t BME280_CHIP_ID = 0x60; //| 0x62 | 0x88;
constexpr uint8_t BME280_RESET_CMD = 0xB6;
constexpr uint8_t STATUS_MEASURING = 1u << 3;
constexpr uint8_t STATUS_IM_UPDATE = 1u << 0;

// ---------------------------------------------------------------------------
// Time base: SysTick, 1 ms
// ---------------------------------------------------------------------------
static volatile uint32_t g_ms;

extern "C" void SysTick_Handler(void)
{
    g_ms = g_ms + 1;
}

static void time_init()
{
    SysTick_Config(PCLK_HZ / 1000);
}

static uint32_t millis()
{
    return g_ms;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) {
    }
}

// ---------------------------------------------------------------------------
// USART1 (debug output)
// ---------------------------------------------------------------------------
static void uart_init()
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // PA9 (TX), PA10 (RX)
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    GPIOA->MODER |= (2u << GPIO_MODER_MODER9_Pos) | (2u << GPIO_MODER_MODER10_Pos);
    GPIOA->AFR[1] &= ~((0xFu << 4) | (0xFu << 8));
    GPIOA->AFR[1] |= (7u << 4) | (7u << 8);

    USART1->BRR = (PCLK_HZ + UART_BAUD / 2) / UART_BAUD;  // oversampling by 16
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) {}
    USART1->DR = static_cast<uint8_t>(c);
}

static void uart_puts(const char* s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

// ---------------------------------------------------------------------------
// I2C1 master driver (polling)
// ---------------------------------------------------------------------------
static void i2c_gpio_init()
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    // PB6 = SCL, PB7 = SDA: alternate function 4, open-drain, high speed, pull-up
    GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER |= (2u << GPIO_MODER_MODER6_Pos) | (2u << GPIO_MODER_MODER7_Pos);
    GPIOB->OTYPER |= GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7;
    GPIOB->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED6_Pos) | (3u << GPIO_OSPEEDR_OSPEED7_Pos);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD6 | GPIO_PUPDR_PUPD7);
    GPIOB->PUPDR |= (1u << GPIO_PUPDR_PUPD6_Pos) | (1u << GPIO_PUPDR_PUPD7_Pos);
    GPIOB->AFR[0] &= ~((0xFu << 24) | (0xFu << 28));
    GPIOB->AFR[0] |= (4u << 24) | (4u << 28);
}

static void i2c_init()
{
    i2c_gpio_init();
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    // Reset the peripheral; also clears a stuck BUSY flag
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;

    // Standard mode 100 kHz:
    //   FREQ  = PCLK1 in MHz
    //   CCR   = PCLK1 / (2 * f_scl)       (Thigh = Tlow = CCR * TPCLK1)
    //   TRISE = 1000 ns / TPCLK1 + 1      (max SCL rise time in standard mode is 1000 ns)
    const uint32_t pclk_mhz = PCLK_HZ / 1000000;
    I2C1->CR2 = pclk_mhz;
    I2C1->CCR = PCLK_HZ / (2 * I2C_SPEED_HZ);
    I2C1->TRISE = pclk_mhz + 1;
    I2C1->CR1 = I2C_CR1_PE;
}

// Wait until (SR1 & mask) != 0. Fails on NACK / bus error / arbitration loss / timeout.
static bool i2c_wait_sr1(uint32_t mask)
{
    uint32_t start = millis();
    while (!(I2C1->SR1 & mask)) {
        if (I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) {
            return false;
        }
        if ((millis() - start) > I2C_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static bool i2c_wait_not_busy()
{
    uint32_t start = millis();
    while (I2C1->SR2 & I2C_SR2_BUSY) {
        if ((millis() - start) > I2C_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

// Release the bus and reset the peripheral after an error
static bool i2c_fail()
{
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 = 0;  // clear AF, BERR, ARLO
    i2c_init();
    return false;
}

static bool i2c_start(uint8_t addr7, bool read)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_sr1(I2C_SR1_SB)) {
        return false;
    }
    // Writing DR after reading SR1 clears SB
    I2C1->DR = static_cast<uint8_t>((addr7 << 1) | (read ? 1 : 0));
    return i2c_wait_sr1(I2C_SR1_ADDR);
}

static void i2c_clear_addr()
{
    // ADDR is cleared by reading SR1 followed by SR2
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

// Write `len` bytes: [START][addr+W][data...][STOP]
static bool i2c_write(uint8_t addr7, const uint8_t* data, uint32_t len)
{
    if (!i2c_wait_not_busy() || !i2c_start(addr7, false)) {
        return i2c_fail();
    }
    i2c_clear_addr();

    for (uint32_t i = 0; i < len; i++) {
        if (!i2c_wait_sr1(I2C_SR1_TXE)) {
            return i2c_fail();
        }
        I2C1->DR = data[i];
    }

    if (!i2c_wait_sr1(I2C_SR1_BTF)) {
        return i2c_fail();
    }
    I2C1->CR1 |= I2C_CR1_STOP;
    return true;
}

// Read `len` bytes (len >= 1) from register `reg`:
// [START][addr+W][reg][RESTART][addr+R][data...][NACK][STOP]
// The N = 1, N = 2 and N > 2 sequences follow the STM32F4 reference manual (I2C master receiver).
static bool i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t* buf, uint32_t len)
{
    if (len == 0 || !i2c_wait_not_busy()) {
        return false;
    }

    // Address phase: select the register
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;
    if (!i2c_start(addr7, false)) {
        return i2c_fail();
    }
    i2c_clear_addr();
    if (!i2c_wait_sr1(I2C_SR1_TXE)) {
        return i2c_fail();
    }
    I2C1->DR = reg;
    if (!i2c_wait_sr1(I2C_SR1_BTF)) {
        return i2c_fail();
    }

    // Repeated start in read mode
    if (len == 2) {
        I2C1->CR1 |= I2C_CR1_POS;  // NACK applies to the byte after the one being received
    }
    if (!i2c_start(addr7, true)) {
        return i2c_fail();
    }

    if (len == 1) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        i2c_clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;
        if (!i2c_wait_sr1(I2C_SR1_RXNE)) {
            return i2c_fail();
        }
        buf[0] = static_cast<uint8_t>(I2C1->DR);
    }
    else if (len == 2) {
        i2c_clear_addr();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        if (!i2c_wait_sr1(I2C_SR1_BTF)) {
            return i2c_fail();
        }
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[0] = static_cast<uint8_t>(I2C1->DR);
        buf[1] = static_cast<uint8_t>(I2C1->DR);
    }
    else {
        i2c_clear_addr();
        uint32_t i = 0;
        // Bytes 0 .. len-4: one RXNE each, ACK stays enabled
        while (len - i > 3) {
            if (!i2c_wait_sr1(I2C_SR1_RXNE)) {
                return i2c_fail();
            }
            buf[i++] = static_cast<uint8_t>(I2C1->DR);
        }
        // Three bytes left: BTF means byte N-2 is in DR and byte N-1 is in the shift register
        if (!i2c_wait_sr1(I2C_SR1_BTF)) {
            return i2c_fail();
        }
        I2C1->CR1 &= ~I2C_CR1_ACK;                  // NACK the last byte
        buf[i++] = static_cast<uint8_t>(I2C1->DR);  // byte N-2
        if (!i2c_wait_sr1(I2C_SR1_BTF)) {
            return i2c_fail();
        }
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[i++] = static_cast<uint8_t>(I2C1->DR);  // byte N-1
        buf[i++] = static_cast<uint8_t>(I2C1->DR);  // byte N
    }

    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;
    return true;
}

static bool i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    const uint8_t frame[2] = {reg, value};
    return i2c_write(addr7, frame, sizeof(frame));
}

// Probe an address: START + address + STOP, true if the device ACKs
static bool i2c_probe(uint8_t addr7)
{
    if (!i2c_wait_not_busy() || !i2c_start(addr7, false)) {
        return i2c_fail();
    }
    i2c_clear_addr();
    I2C1->CR1 |= I2C_CR1_STOP;
    return true;
}

// ---------------------------------------------------------------------------
// BME280 driver (integer compensation formulas from the Bosch datasheet, section 4.2.3)
// ---------------------------------------------------------------------------
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

static Bme280Calib g_cal;
static int32_t g_t_fine;

static uint16_t le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static bool bme280_read_calibration()
{
    uint8_t a[24];
    uint8_t h1;
    uint8_t b[7];

    if (!i2c_read_reg(BME280_ADDR, REG_CALIB_00, a, sizeof(a)) || !i2c_read_reg(BME280_ADDR, REG_CALIB_H1, &h1, 1) || !i2c_read_reg(BME280_ADDR, REG_CALIB_26, b, sizeof(b))) {
        return false;
    }

    g_cal.T1 = le16(&a[0]);
    g_cal.T2 = static_cast<int16_t>(le16(&a[2]));
    g_cal.T3 = static_cast<int16_t>(le16(&a[4]));
    g_cal.P1 = le16(&a[6]);
    g_cal.P2 = static_cast<int16_t>(le16(&a[8]));
    g_cal.P3 = static_cast<int16_t>(le16(&a[10]));
    g_cal.P4 = static_cast<int16_t>(le16(&a[12]));
    g_cal.P5 = static_cast<int16_t>(le16(&a[14]));
    g_cal.P6 = static_cast<int16_t>(le16(&a[16]));
    g_cal.P7 = static_cast<int16_t>(le16(&a[18]));
    g_cal.P8 = static_cast<int16_t>(le16(&a[20]));
    g_cal.P9 = static_cast<int16_t>(le16(&a[22]));

    g_cal.H1 = h1;
    g_cal.H2 = static_cast<int16_t>(le16(&b[0]));
    g_cal.H3 = b[2];
    // H4 = 0xE4[7:0] : 0xE5[3:0], H5 = 0xE6[7:0] : 0xE5[7:4] (12-bit signed values)
    g_cal.H4 = static_cast<int16_t>((static_cast<int8_t>(b[3]) * 16) | (b[4] & 0x0F));
    g_cal.H5 = static_cast<int16_t>((static_cast<int8_t>(b[5]) * 16) | (b[4] >> 4));
    g_cal.H6 = static_cast<int8_t>(b[6]);
    return true;
}

// Returns temperature in 0.01 degC. Also updates g_t_fine used by the other two compensations.
static int32_t bme280_comp_temp(int32_t adc_T)
{
    int32_t var1 = (((adc_T >> 3) - (static_cast<int32_t>(g_cal.T1) << 1)) * static_cast<int32_t>(g_cal.T2)) >> 11;
    int32_t d = (adc_T >> 4) - static_cast<int32_t>(g_cal.T1);
    int32_t var2 = (((d * d) >> 12) * static_cast<int32_t>(g_cal.T3)) >> 14;
    g_t_fine = var1 + var2;
    return (g_t_fine * 5 + 128) >> 8;
}

// Returns pressure in Pa as unsigned Q24.8 (value / 256 = Pa)
static uint32_t bme280_comp_press(int32_t adc_P)
{
    int64_t var1 = static_cast<int64_t>(g_t_fine) - 128000;
    int64_t var2 = var1 * var1 * g_cal.P6;
    var2 += (var1 * g_cal.P5) << 17;
    var2 += static_cast<int64_t>(g_cal.P4) << 35;
    var1 = ((var1 * var1 * g_cal.P3) >> 8) + ((var1 * g_cal.P2) << 12);
    var1 = (((static_cast<int64_t>(1) << 47) + var1) * g_cal.P1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (static_cast<int64_t>(g_cal.P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<int64_t>(g_cal.P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(g_cal.P7) << 4);
    return static_cast<uint32_t>(p);
}

// Returns relative humidity as unsigned Q22.10 (value / 1024 = %RH)
static uint32_t bme280_comp_hum(int32_t adc_H)
{
    int32_t v = g_t_fine - 76800;
    v =
        (((((adc_H << 14) - (static_cast<int32_t>(g_cal.H4) << 20) - (static_cast<int32_t>(g_cal.H5) * v)) + 16384) >> 15) *
         (((((((v * static_cast<int32_t>(g_cal.H6)) >> 10) * (((v * static_cast<int32_t>(g_cal.H3)) >> 11) + 32768)) >> 10) + 2097152) * static_cast<int32_t>(g_cal.H2) + 8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * static_cast<int32_t>(g_cal.H1)) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return static_cast<uint32_t>(v >> 12);
}

static bool bme280_init()
{
    uint8_t id = 0;
    if (!i2c_read_reg(BME280_ADDR, REG_ID, &id, 1) || id != BME280_CHIP_ID) {
        return false;
    }

    if (!i2c_write_reg(BME280_ADDR, REG_RESET, BME280_RESET_CMD)) {
        return false;
    }
    delay_ms(5);

    // Wait until the NVM calibration copy has finished (im_update = 0)
    uint8_t status = STATUS_IM_UPDATE;
    uint32_t start = millis();
    while (status & STATUS_IM_UPDATE) {
        if (!i2c_read_reg(BME280_ADDR, REG_STATUS, &status, 1) || (millis() - start) > 100) {
            return false;
        }
    }

    if (!bme280_read_calibration()) {
        return false;
    }

    // IIR filter off; standby time is irrelevant in forced mode
    return i2c_write_reg(BME280_ADDR, REG_CONFIG, 0x00);
}

struct Bme280Sample {
    int32_t temp_c100;  // 0.01 degC
    uint32_t press_q8;  // Pa * 256
    uint32_t hum_q10;   // %RH * 1024
};

// Trigger one forced-mode conversion (oversampling x4 for T, P and H) and read the result
static bool bme280_measure(Bme280Sample& out)
{
    // ctrl_hum is only applied after a write to ctrl_meas, so write it first
    if (!i2c_write_reg(BME280_ADDR, REG_CTRL_HUM, 0x03)) { //18
        return false;
    }
    // osrs_t = x4 (011), osrs_p = x4 (011), mode = forced (01)
    if (!i2c_write_reg(BME280_ADDR, REG_CTRL_MEAS, (3u << 5) | (3u << 2) | 1u)) {
        return false;
    }

    uint8_t status = STATUS_MEASURING;
    uint32_t start = millis();
    delay_ms(2);
    while (status & STATUS_MEASURING) {
        if (!i2c_read_reg(BME280_ADDR, REG_STATUS, &status, 1) || (millis() - start) > 100) {
            return false;
        }
    }

    // Burst read: press[19:12], press[11:4], press[3:0], temp[19:12], temp[11:4], temp[3:0], hum[15:8], hum[7:0]
    uint8_t d[8];
    if (!i2c_read_reg(BME280_ADDR, REG_DATA, d, sizeof(d))) {
        return false;
    }

    int32_t adc_P = (static_cast<int32_t>(d[0]) << 12) | (static_cast<int32_t>(d[1]) << 4) | (d[2] >> 4);
    int32_t adc_T = (static_cast<int32_t>(d[3]) << 12) | (static_cast<int32_t>(d[4]) << 4) | (d[5] >> 4);
    int32_t adc_H = (static_cast<int32_t>(d[6]) << 8) | d[7];

    out.temp_c100 = bme280_comp_temp(adc_T);  // must run first: it computes g_t_fine
    out.press_q8 = bme280_comp_press(adc_P);
    out.hum_q10 = bme280_comp_hum(adc_H);
    return true;
}

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------
static void i2c_scan()
{
    uart_puts("I2C scan:\r\n");
    char line[32];
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_probe(addr)) {
            snprintf(line, sizeof(line), "  found 0x%02X\r\n", addr);
            uart_puts(line);
        }
    }
}

// Printed with integer math only, so no float printf support is needed
static void print_sample(const Bme280Sample& s)
{
    int32_t t = s.temp_c100;
    uint32_t t_abs = static_cast<uint32_t>(t < 0 ? -t : t);
    uint32_t hpa_x100 = s.press_q8 / 256;  // Pa == hPa * 100
    uint32_t h_x100 = static_cast<uint32_t>((static_cast<uint64_t>(s.hum_q10) * 100) / 1024);

    char line[96];
    snprintf(
        line,
        sizeof(line),
        "T = %s%lu.%02lu C, P = %lu.%02lu hPa, H = %lu.%02lu %%RH\r\n",
        t < 0 ? "-" : "",
        static_cast<unsigned long>(t_abs / 100),
        static_cast<unsigned long>(t_abs % 100),
        static_cast<unsigned long>(hpa_x100 / 100),
        static_cast<unsigned long>(hpa_x100 % 100),
        static_cast<unsigned long>(h_x100 / 100),
        static_cast<unsigned long>(h_x100 % 100)
    );
    uart_puts(line);
}

int main()
{
    time_init();
    uart_init();
    i2c_init();

    delay_ms(100);  // uart start-up time
    uart_puts("\r\nSTM32F411 bare-metal I2C1 + BME280 (SDA=PB7, SCL=PB6)\r\n");
    i2c_scan();

    while (!bme280_init()) {
        uart_puts("BME280 not found or init failed, retrying...\r\n");
        delay_ms(1000);  // BME280 start-up time
    }
    uart_puts("BME280 ready\r\n");

    while (true) {
        Bme280Sample s;
        if (bme280_measure(s)) {
            print_sample(s);
        }
        else {
            uart_puts("BME280 read error\r\n");
        }
        delay_ms(1000);
    }
}
