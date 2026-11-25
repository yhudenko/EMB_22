// ============================================================================
// DMA ДЕМО 5: ADC + DMA, кілька входів одночасно     (STM32F411 Black Pill)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ (напруга на входах лише 0..3.3 В!)
//   PA1 -> 3V3                     (має показати ~VDDA)
//   PA2 -> GND                     (~0 мВ)
//   PA3 -> середній вивід потенціометра 10 кОм (крайні - на 3V3 і GND),
//          або нічого (вхід "висить" - побачите шум)
//   Вивід - Serial monitor через USB-C (115200).
//
// ЯК ЦЕ ПРАЦЮЄ
//   ADC1 у режимі SCAN + CONTINUOUS сам по колу перетворює послідовність
//   ("regular sequence") із 5 каналів:
//     rank 1: PA1 (IN1)   rank 2: PA2 (IN2)   rank 3: PA3 (IN3)
//     rank 4: VREFINT (IN17) - внутрішнє опорне ~1.21 В, з нього рахуємо реальну VDDA
//     rank 5: TEMP    (IN18) - внутрішній датчик температури кристала
//   Після КОЖНОГО перетворення ADC смикає DMA, і DMA кладе 16-бітний результат
//   у наступну комірку масиву adcBuf. CPU у цьому участі не бере.
//
//   adcBuf (кільцевий, DMA_CIRCULAR), 64 кадри по 5 каналів:
//     [PA1 PA2 PA3 VREF TEMP][PA1 PA2 PA3 VREF TEMP] ... [PA1 PA2 PA3 VREF TEMP]
//     |<------- перша половина ------->|<------- друга половина ------->|
//            HalfCplt callback                    Cplt callback
//   Дійшовши до кінця, DMA сам починає спочатку. Поки він пише одну половину,
//   ми спокійно обробляємо іншу ("подвійна буферизація", ping-pong).
//
// А БЕЗ DMA?
//   У ADC1 лише ОДИН регістр результату ADC1->DR. У режимі SCAN наступне
//   перетворення перезаписує попереднє: не встиг CPU прочитати - дані втрачено
//   (помилка OVR). Тому кілька каналів у scan-режимі читають саме через DMA.
//
// ЧАСТОТА
//   ADC тактується PCLK2 / 4 = 96 / 4 = 24 МГц. Одне перетворення =
//   480 (вибірка) + 12 тактів = 492 такти = 20.5 мкс; 5 каналів = 102.5 мкс
//   -> ~9750 кадрів/с = ~48 750 вимірювань/с. А CPU отримує лише ~300
//   переривань/с (2 на 64 кадри) і підсумовує вже готові числа.
//
// РЕСУРСИ STM32F411: ADC1 = DMA2 Stream0 Channel0 (альтернатива - Stream4 Channel0).
// ============================================================================
#include <Arduino.h>

constexpr uint8_t CH_NUM = 5;    // скільки каналів у послідовності
constexpr uint16_t FRAMES = 64;  // кадр = один прохід по всіх каналах

enum { CH_PA1, CH_PA2, CH_PA3, CH_VREF, CH_TEMP };
static const char* const CH_NAMES[CH_NUM] = {"PA1", "PA2", "PA3", "VREFINT", "TEMP"};
static const uint32_t CH_ADC[CH_NUM] = {ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_VREFINT, ADC_CHANNEL_TEMPSENSOR};

// Заводські калібрування (ST вимірює їх при VDDA = 3.3 В) лежать у system memory:
//   VREFINT_CAL_ADDR 0x1FFF7A2A, TEMPSENSOR_CAL1_ADDR 0x1FFF7A2C (30 C),
//   TEMPSENSOR_CAL2_ADDR 0x1FFF7A2E (110 C) - макроси з stm32f4xx_ll_adc.h.
// На клонах чипа там може бути сміття - тоді температура буде неправдоподібна.

static ADC_HandleTypeDef hadc1;
static DMA_HandleTypeDef hdmaAdc;
static uint16_t adcBuf[FRAMES * CH_NUM];  // сюди пише DMA

// Статистика, яку накопичують callback-и (з переривання)
struct Stats {
    uint32_t sum[CH_NUM];
    uint16_t min[CH_NUM];
    uint16_t max[CH_NUM];
    uint32_t frames;
    uint32_t irqs;
    uint32_t cpuCycles;  // скільки тактів CPU провів в обробнику DMA
};
static Stats acc;
static volatile bool adcError = false;

static void statsReset(Stats& s)
{
    memset(&s, 0, sizeof(s));
    for (auto& m : s.min) {
        m = 0xFFFF;
    }
}

// ---- Обробники переривань -> HAL
extern "C" void DMA2_Stream0_IRQHandler(void)
{
    const uint32_t c0 = DWT->CYCCNT;  // лічильник тактів CPU
    HAL_DMA_IRQHandler(&hdmaAdc);     // -> HAL_ADC_ConvHalfCpltCallback / HAL_ADC_ConvCpltCallback
    acc.cpuCycles += DWT->CYCCNT - c0;
}

extern "C" void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);  // HAL вмикає переривання OVR (переповнення)
}

// Підсумувати половину буфера (FRAMES/2 кадрів)
static void accumulate(const uint16_t* p)
{
    for (uint16_t f = 0; f < FRAMES / 2; f++) {
        for (uint8_t ch = 0; ch < CH_NUM; ch++) {
            const uint16_t v = *p++;
            acc.sum[ch] += v;
            if (v < acc.min[ch]) {
                acc.min[ch] = v;
            }
            if (v > acc.max[ch]) {
                acc.max[ch] = v;
            }
        }
    }
    acc.frames += FRAMES / 2;
    acc.irqs++;
}

// ---- Callback-и HAL: викликаються з переривання DMA
extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* h)
{
    if (h->Instance == ADC1) {
        accumulate(&adcBuf[0]);  // DMA заповнив першу половину і пише другу
    }
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* h)
{
    if (h->Instance == ADC1) {
        accumulate(&adcBuf[FRAMES / 2 * CH_NUM]);  // заповнив другу, знову пише першу
    }
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* h)
{
    if (h->Instance == ADC1) {
        adcError = true;
    }
}

static void adcInit()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    // PA1..PA3 - аналоговий режим (цифровий вхідний буфер вимкнено)
    GPIO_InitTypeDef g = {};
    g.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    hdmaAdc.Instance = DMA2_Stream0;
    hdmaAdc.Init.Channel = DMA_CHANNEL_0;
    hdmaAdc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdmaAdc.Init.PeriphInc = DMA_PINC_DISABLE;                  // завжди той самий ADC1->DR
    hdmaAdc.Init.MemInc = DMA_MINC_ENABLE;                      // по масиву - вперед
    hdmaAdc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  // 12-бітний результат у 16-бітному слові
    hdmaAdc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdmaAdc.Init.Mode = DMA_CIRCULAR;  // по колу, без зупинки
    hdmaAdc.Init.Priority = DMA_PRIORITY_HIGH;
    hdmaAdc.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdmaAdc);
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdmaAdc);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;  // 96 / 4 = 24 МГц (макс. 36)
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;        // SCAN: усі канали послідовності по черзі
    hadc1.Init.ContinuousConvMode = ENABLE;  // CONTINUOUS: після останнього - знову з першого
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = CH_NUM;
    hadc1.Init.DMAContinuousRequests = ENABLE;  // потрібно для DMA_CIRCULAR
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef c = {};
    for (uint8_t i = 0; i < CH_NUM; i++) {
        c.Channel = CH_ADC[i];
        c.Rank = i + 1;
        // Довга вибірка: датчику температури треба >= 10 мкс, а зовнішнім
        // джерелам з великим опором (потенціометр) - встигнути зарядити конденсатор ADC
        c.SamplingTime = ADC_SAMPLETIME_480CYCLES;
        HAL_ADC_ConfigChannel(&hadc1, &c);  // для VREFINT/TEMP HAL сам вмикає TSVREFE
    }

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority(ADC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
}

static bool adcStart()
{
    statsReset(acc);
    // Один виклик - і далі ADC + DMA працюють самі, поки їх не зупинять
    return HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuf, FRAMES * CH_NUM) == HAL_OK;
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
    }
    Serial.println("\nDMA demo 5: ADC1 scan (PA1, PA2, PA3, VREFINT, TEMP) -> DMA2 Stream0, circular");

    // Лічильник тактів CPU (DWT) - щоб виміряти, скільки часу забирають переривання
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    adcInit();
    if (!adcStart()) {
        Serial.println("ADC/DMA start failed");
    }
}

void loop()
{
    static uint32_t lastUs = micros();

    // CPU тут просто чекає (або робив би щось своє). DMA тим часом
    // ~9750 разів на секунду заповнює adcBuf, а callback-и підсумовують.
    delay(1000);

    if (adcError) {
        adcError = false;
        HAL_ADC_Stop_DMA(&hadc1);
        Serial.println("ADC error (overrun?) -> restart");
        adcStart();
        lastUs = micros();
        return;
    }

    // Забрати накопичене і почати заново. Переривання вимикаємо на мить,
    // щоб callback не змінив acc посеред копіювання.
    Stats s;
    __disable_irq();
    s = acc;
    statsReset(acc);
    __enable_irq();
    const uint32_t nowUs = micros();
    const uint32_t elapsedUs = nowUs - lastUs;
    lastUs = nowUs;

    if (s.frames == 0) {
        Serial.println("No data from DMA");
        return;
    }

    uint32_t avg[CH_NUM];
    for (uint8_t ch = 0; ch < CH_NUM; ch++) {
        avg[ch] = (s.sum[ch] + s.frames / 2) / s.frames;
    }

    // Реальна напруга живлення ADC: VREFINT_CAL виміряли при 3300 мВ
    const uint32_t vdda = avg[CH_VREF] ? 3300UL * *VREFINT_CAL_ADDR / avg[CH_VREF] : 3300;

    const uint32_t cpuUs = s.cpuCycles / (SystemCoreClock / 1000000);
    const uint32_t cpuPermille = cpuUs * 1000 / elapsedUs;
    Serial.printf("\n----- %lu ms: %lu frames x %u ch = %lu samples by DMA, %lu IRQs, CPU in IRQ %lu us (%lu.%lu%%) -----\n", (unsigned long)(elapsedUs / 1000), (unsigned long)s.frames, CH_NUM, (unsigned long)(s.frames * CH_NUM), (unsigned long)s.irqs, (unsigned long)cpuUs, (unsigned long)(cpuPermille / 10), (unsigned long)(cpuPermille % 10));

    for (uint8_t ch = 0; ch < CH_NUM; ch++) {
        Serial.printf("  %-7s: avg %4lu = %4lu mV   (min %4u, max %4u)\n", CH_NAMES[ch], (unsigned long)avg[ch], (unsigned long)(avg[ch] * vdda / 4095), s.min[ch], s.max[ch]);
    }

    // Температура: калібрування теж зроблене при 3.3 В -> перераховуємо відлік до 3.3 В
    const float ts = (float)avg[CH_TEMP] * vdda / 3300.0f;
    const float cal1 = *TEMPSENSOR_CAL1_ADDR;
    const float cal2 = *TEMPSENSOR_CAL2_ADDR;
    const float tempC = TEMPSENSOR_CAL1_TEMP + (ts - cal1) * (TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP) / (cal2 - cal1);

    Serial.printf("  VDDA = %lu mV (from VREFINT),  chip temp = ", (unsigned long)vdda);
    Serial.print(tempC, 1);
    Serial.println(" C");
}
