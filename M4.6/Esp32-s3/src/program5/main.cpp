// ============================================================================
// DMA ДЕМО 5: ADC + DMA, кілька входів одночасно     (ESP32-S3)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ (напруга на входах лише 0..3.3 В!)
//   GPIO4 -> 3V3                   (~3100+ мВ, верх діапазону при 12 дБ)
//   GPIO5 -> GND                   (~0 мВ)
//   GPIO6 -> середній вивід потенціометра 10 кОм (крайні - на 3V3 і GND)
//   GPIO7 -> нічого (вхід "висить" - побачите шум)
//   Вивід - Serial0 (115200).
//
// ЯК ЦЕ ПРАЦЮЄ ("continuous mode" / "DMA mode" АЦП)
//   У SAR ADC ESP32-S3 є цифровий контролер (DIG). Йому дають таблицю каналів
//   ("pattern") і частоту - і він сам по колу запускає перетворення:
//     CH3 (GPIO4) -> CH4 (GPIO5) -> CH5 (GPIO6) -> CH6 (GPIO7) -> CH3 -> ...
//   Кожен результат (4 байти: 12 біт даних + номер каналу + номер ADC) GDMA
//   кладе в пам'ять. Коли набереться conv_num_each_intr байтів - переривання;
//   драйвер перекладає порцію у свій кільцевий буфер (ringbuffer), а ми
//   забираємо її через adc_digi_read_bytes().
//
//   analogRead():   CPU [старт][чекаю...][читаю] - на КОЖНЕ вимірювання
//   DMA:            CPU лише ~300 разів/с забирає готові порції по 64 вимірювання
//
// ЩО РОБИТЬ ПРОГРАМА
//   Безперервно забирає дані, розкладає по каналах і раз на секунду друкує
//   кількість вимірювань, середнє / мін / макс (сирі та в мВ) для кожного входу.
//
// НА ЩО ЗВЕРНУТИ УВАГУ
//   - Лише ADC1 (GPIO1..10). ADC2 ділить ресурси з Wi-Fi.
//   - Номер каналу приходить РАЗОМ із даними, тому розкладаємо результати
//     саме за ним, а не за позицією в буфері.
//   - adc_digi_read_bytes() із таймаутом БЛОКУЄ задачу (FreeRTOS), а не крутить
//     CPU в циклі: поки даних немає, CPU вільний для інших задач.
//   - Драйвер тут - API ESP-IDF 4.4 (driver/adc.h), бо Arduino-ESP32 2.x
//     зібраний на IDF 4.4. В IDF 5 / Arduino 3.x те саме робить
//     esp_adc/adc_continuous.h (або Arduino-функції analogContinuous*()).
// ============================================================================
#include <Arduino.h>

#include "driver/adc.h"
#include "esp_adc_cal.h"

constexpr int CH_NUM = 4;
static const uint8_t ADC_CH[CH_NUM] = {3, 4, 5, 6};   // ADC1_CHANNEL_n
static const uint8_t GPIO_OF[CH_NUM] = {4, 5, 6, 7};  // на S3: ADC1_CHn = GPIO(n+1)

constexpr uint32_t SAMPLE_HZ = 20000;  // сумарно на всі канали -> ~5000/с на канал
constexpr uint32_t RESULT_BYTES = 4;   // розмір одного результату (формат TYPE2)
constexpr uint32_t CHUNK_BYTES = 256;  // порція на одне переривання = 64 результати

alignas(4) static uint8_t chunk[CHUNK_BYTES];
static esp_adc_cal_characteristics_t adcCal;  // для перерахунку сирих відліків у мВ

struct Stats {
    uint32_t sum[CH_NUM];
    uint32_t count[CH_NUM];
    uint16_t min[CH_NUM];
    uint16_t max[CH_NUM];
    uint32_t invalid;  // результати з "чужим" номером каналу
    uint32_t cpuUs;    // скільки CPU витратив на розбір даних
    bool overflow;     // буфер драйвера переповнювався (ми не встигали забирати)
};
static Stats st;

static void statsReset()
{
    memset(&st, 0, sizeof(st));
    for (auto& m : st.min) {
        m = 0xFFFF;
    }
}

static void adcInit()
{
    adc_digi_init_config_t init = {};
    init.max_store_buf_size = 8192;  // ringbuffer драйвера (~100 мс даних про запас)
    init.conv_num_each_intr = CHUNK_BYTES;
    for (int i = 0; i < CH_NUM; i++) {
        init.adc1_chan_mask |= BIT(ADC_CH[i]);
    }
    init.adc2_chan_mask = 0;
    ESP_ERROR_CHECK(adc_digi_initialize(&init));  // також налаштовує GPIO як аналогові

    static adc_digi_pattern_config_t pattern[CH_NUM] = {};
    for (int i = 0; i < CH_NUM; i++) {
        pattern[i].atten = ADC_ATTEN_DB_12;  // діапазон ~0..3.1 В
        pattern[i].channel = ADC_CH[i];
        pattern[i].unit = 0;  // 0 = ADC1
        pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }

    adc_digi_configuration_t cfg = {};
    cfg.conv_limit_en = false;
    cfg.conv_limit_num = 250;
    cfg.pattern_num = CH_NUM;
    cfg.adc_pattern = pattern;
    cfg.sample_freq_hz = SAMPLE_HZ;
    cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&cfg));

    // Заводське калібрування з eFuse: сирий відлік -> мВ
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adcCal);
}

// Розкласти порцію результатів по каналах
static void parse(const uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i + RESULT_BYTES <= len; i += RESULT_BYTES) {
        const adc_digi_output_data_t* d = (const adc_digi_output_data_t*)&buf[i];
        const uint32_t ch = d->type2.channel;
        const uint16_t v = d->type2.data;

        int idx = -1;
        for (int k = 0; k < CH_NUM; k++) {
            if (ADC_CH[k] == ch) {
                idx = k;
                break;
            }
        }
        if (d->type2.unit != 0 || idx < 0) {
            st.invalid++;
            continue;
        }
        st.sum[idx] += v;
        st.count[idx]++;
        if (v < st.min[idx]) {
            st.min[idx] = v;
        }
        if (v > st.max[idx]) {
            st.max[idx] = v;
        }
    }
}

static void printStats(uint32_t elapsedMs)
{
    uint32_t total = 0;
    for (int i = 0; i < CH_NUM; i++) {
        total += st.count[i];
    }
    const uint32_t cpuPermille = st.cpuUs / (elapsedMs ? elapsedMs : 1);  // мкс/мс = проміле
    Serial0.printf("\n----- %lu ms: %lu samples by DMA, CPU parsing %lu us (%lu.%lu%%)%s -----\n", (unsigned long)elapsedMs, (unsigned long)total, (unsigned long)st.cpuUs, (unsigned long)(cpuPermille / 10), (unsigned long)(cpuPermille % 10), st.overflow ? ", OVERFLOW: data lost!" : "");

    for (int i = 0; i < CH_NUM; i++) {
        if (st.count[i] == 0) {
            Serial0.printf("  GPIO%-2u (ADC1_CH%u): no data\n", GPIO_OF[i], ADC_CH[i]);
            continue;
        }
        const uint32_t avg = (st.sum[i] + st.count[i] / 2) / st.count[i];
        Serial0.printf("  GPIO%-2u (ADC1_CH%u): avg %4lu = %4lu mV   (min %4u, max %4u, %lu samples)\n", GPIO_OF[i], ADC_CH[i], (unsigned long)avg, (unsigned long)esp_adc_cal_raw_to_voltage(avg, &adcCal), st.min[i], st.max[i], (unsigned long)st.count[i]);
    }
    if (st.invalid) {
        Serial0.printf("  invalid results: %lu\n", (unsigned long)st.invalid);
    }
}

void setup()
{
    Serial0.begin(115200);
    delay(500);
    Serial0.println("\nDMA demo 5: ADC1 continuous mode (GPIO4..7) -> GDMA");
    adcInit();
    statsReset();
    ESP_ERROR_CHECK(adc_digi_start());  // далі ADC + DMA працюють самі
}

void loop()
{
    static uint32_t lastPrint = millis();

    // Забрати те, що DMA вже наносив (чекаємо до 100 мс, якщо поки порожньо)
    uint32_t len = 0;
    const esp_err_t r = adc_digi_read_bytes(chunk, sizeof(chunk), &len, 100);
    if (r == ESP_ERR_INVALID_STATE) {
        st.overflow = true;  // дані все одно повернуто, але частину раніше втрачено
    }
    if (len > 0) {
        const uint32_t t0 = micros();
        parse(chunk, len);
        st.cpuUs += micros() - t0;
    }

    const uint32_t now = millis();
    if (now - lastPrint >= 1000) {
        printStats(now - lastPrint);
        statsReset();
        lastPrint = now;
    }
}
