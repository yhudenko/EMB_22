// ============================================================================
// DMA ДЕМО 1: ПАМ'ЯТЬ -> ПАМ'ЯТЬ          (ESP32-S3)
// ============================================================================
//
// ЩО ТАКЕ DMA (Direct Memory Access, "прямий доступ до пам'яті")
//   Без DMA всі дані переносить процесор (CPU): "прочитай байт звідси, запиши
//   туди" - і так тисячі разів. Поки CPU це робить, нічим іншим він не займається.
//   DMA - окремий маленький "вантажник" усередині мікроконтролера. CPU лише дає
//   йому завдання: ЗВІДКИ, КУДИ і СКІЛЬКИ - і йде робити своє. DMA копіює сам,
//   а коли закінчить - повідомляє CPU перериванням (викликає нашу функцію-callback).
//
//     без DMA:  CPU: [копіюю][копіюю][копіюю][копіюю] -> вільний
//     з DMA:    CPU: [дав завдання] -> вільний, робить інше ...... [callback: готово]
//               DMA:                [копіюю][копіюю][копіюю][копіюю]
//
// DMA В ESP32-S3 (GDMA, "General DMA")
//   Один спільний контролер на 5 пар каналів (TX + RX). Кожен канал можна
//   "під'єднати" до периферії (SPI, I2S, LCD/CAM, ADC, UHCI-UART, AES, SHA)
//   або до пам'яті. Завдання описуються "дескрипторами" - маленькими
//   структурами в RAM: адреса буфера + довжина + посилання на наступний.
//   Тут усю цю роботу робить готовий драйвер esp_async_memcpy (ESP-IDF).
//
// ЩО РОБИТЬ ПРОГРАМА (раз на 3 секунди)
//   Копіює 32 КБ трьома способами і порівнює час:
//     1) CPU, по одному байту (звичайний цикл for)
//     2) CPU, memcpy() (бібліотечна функція, дуже оптимізована)
//     3) DMA, а CPU тим часом виконує "іншу роботу" (просто рахує)
//
// ПІДКЛЮЧЕННЯ: нічого. Вивід - Serial0 (115200).
//
// НА ЩО ЗВЕРНУТИ УВАГУ
//   - На ESP32-S3 memcpy() (CPU 240 МГц) зазвичай ШВИДШИЙ за DMA! Перевага
//     DMA не в швидкості, а в тому, що CPU в цей час вільний.
//   - Буфери для DMA мають лежати у "DMA-capable" пам'яті (внутрішня SRAM),
//     тому виділяємо їх через heap_caps_malloc(..., MALLOC_CAP_DMA).
// ============================================================================
#include <Arduino.h>

#include "esp_async_memcpy.h"
#include "esp_heap_caps.h"

constexpr size_t BYTES = 32 * 1024;

static uint8_t* srcBuf = nullptr;
static uint8_t* dstBuf = nullptr;
static async_memcpy_t dmaCopier = nullptr;  // "ручка" (handle) драйвера DMA-копіювання
static volatile bool dmaDone = false;       // volatile: змінюється в перериванні

// Callback: викликається з ПЕРЕРИВАННЯ, коли DMA все скопіював.
// Тут лише ставимо прапорець - у перериванні не можна робити нічого довгого.
// IRAM_ATTR = код лежить у швидкій внутрішній RAM (вимога для обробників переривань).
static bool IRAM_ATTR onDmaDone(async_memcpy_t, async_memcpy_event_t*, void*)
{
    dmaDone = true;
    return false;  // "чи розбудили ми важливішу задачу FreeRTOS" - ні
}

// Найпростіше копіювання: по байту. volatile не дає компілятору "розумно"
// замінити цей цикл на memcpy - хочемо побачити саме повільний варіант.
static void __attribute__((noinline)) cpuCopyBytes(uint8_t* dst, const uint8_t* src, size_t n)
{
    volatile uint8_t* d = dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = src[i];
    }
}

static bool dataOk()
{
    return memcmp(srcBuf, dstBuf, BYTES) == 0;
}

// totalUs - скільки тривало копіювання; busyUs - скільки з цього часу CPU був зайнятий
static void printResult(const char* name, uint32_t totalUs, uint32_t busyUs, bool ok)
{
    const uint32_t safeTotal = totalUs ? totalUs : 1;
    Serial0.printf(
        "%-22s %5lu us (%3lu MB/s) | CPU busy %5lu us (%3lu%%) | data %s\n",
        name,
        (unsigned long)totalUs,
        (unsigned long)(BYTES / safeTotal),  // байт/мкс = МБ/с
        (unsigned long)busyUs,
        (unsigned long)(busyUs * 100 / safeTotal),
        ok ? "OK" : "WRONG!"
    );
}

void setup()
{
    Serial0.begin(115200);
    delay(500);
    Serial0.println("\nDMA demo 1: memory -> memory (ESP32-S3, GDMA via esp_async_memcpy)");

    srcBuf = (uint8_t*)heap_caps_malloc(BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    dstBuf = (uint8_t*)heap_caps_malloc(BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
    // Один дескриптор описує до ~4 КБ, тож на 32 КБ потрібно 8+ дескрипторів.
    // backlog = скільки дескрипторів драйвер виділить (з запасом).
    cfg.backlog = 16;
    // Буфери вирівняні на 4 байти -> DMA може читати "пакетами" (burst), так швидше
    cfg.sram_trans_align = 4;

    if (!srcBuf || !dstBuf || esp_async_memcpy_install(&cfg, &dmaCopier) != ESP_OK) {
        Serial0.println("Init failed (no DMA memory?)");
        while (true) {
            delay(1000);
        }
    }
}

void loop()
{
    static uint32_t round = 0;
    round++;

    // Щоразу нові дані в джерелі - щоб перевірка "data OK" була чесною
    for (size_t i = 0; i < BYTES; i++) {
        srcBuf[i] = (uint8_t)(i * 31 + round);
    }
    Serial0.printf("\n===== Round %lu: copy %u bytes =====\n", (unsigned long)round, (unsigned)BYTES);

    // ---- 1) CPU по байту. CPU зайнятий 100% часу.
    memset(dstBuf, 0, BYTES);
    uint32_t t0 = micros();
    cpuCopyBytes(dstBuf, srcBuf, BYTES);
    uint32_t t = micros() - t0;
    printResult("1) CPU byte loop", t, t, dataOk());

    // ---- 2) CPU memcpy. Швидше, але CPU все одно зайнятий 100% часу.
    memset(dstBuf, 0, BYTES);
    t0 = micros();
    memcpy(dstBuf, srcBuf, BYTES);
    t = micros() - t0;
    printResult("2) CPU memcpy()", t, t, dataOk());

    // ---- 3) DMA. CPU лише запускає передачу, далі вільний.
    memset(dstBuf, 0, BYTES);
    dmaDone = false;
    uint32_t work = 0;

    t0 = micros();
    // Завдання для DMA: куди, звідки, скільки байтів, кого покликати в кінці
    const esp_err_t err = esp_async_memcpy(dmaCopier, dstBuf, srcBuf, BYTES, onDmaDone, nullptr);
    const uint32_t busy = micros() - t0;

    // Поки DMA працює - CPU робить "інше". У реальній програмі тут була б
    // обробка кнопок, дисплей, обчислення... У нас - простий лічильник.
    while (err == ESP_OK && !dmaDone) {
        work++;
    }
    t = micros() - t0;

    if (err != ESP_OK) {
        Serial0.printf("3) DMA error: %s\n", esp_err_to_name(err));
    }
    else {
        printResult("3) DMA", t, busy, dataOk());
        Serial0.printf("   while DMA was copying, CPU did %lu loops of other work\n", (unsigned long)work);
    }

    delay(3000);
}
