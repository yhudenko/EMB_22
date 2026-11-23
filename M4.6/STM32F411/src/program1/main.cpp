// ============================================================================
// DMA ДЕМО 1: ПАМ'ЯТЬ -> ПАМ'ЯТЬ          (STM32F411 Black Pill)
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
// ЩО РОБИТЬ ПРОГРАМА (раз на 3 секунди)
//   Копіює 16 КБ трьома способами і порівнює час:
//     1) CPU, по одному байту (звичайний цикл for)
//     2) CPU, memcpy() (бібліотечна функція: копіює по 4 байти, оптимізована)
//     3) DMA. Поки DMA копіює, CPU виконує "іншу роботу" (просто рахує) і час від
//        часу підглядає в лічильник DMA - скільки слів ще залишилось скопіювати.
//        Видно, як лічильник зменшується сам, без участі CPU.
//
// ПІДКЛЮЧЕННЯ: нічого. Вивід - Serial monitor через USB-C (115200).
//
// НА ЩО ЗВЕРНУТИ УВАГУ
//   - DMA НЕ обов'язково швидший за memcpy! Його головна перевага - CPU вільний.
//   - На STM32F4 є два DMA (DMA1, DMA2) по 8 "потоків" (stream). Копіювати
//     пам'ять -> пам'ять уміє ТІЛЬКИ DMA2.
//   - Одна передача - до 65535 елементів; елемент = 1, 2 або 4 байти.
// ============================================================================
#include <Arduino.h>

constexpr uint32_t WORDS = 4096;          // скільки 32-бітних слів копіюємо
constexpr uint32_t BYTES = WORDS * 4;     // = 16384 байти = 16 КБ
static uint32_t srcBuf[WORDS];            // звідки
static uint32_t dstBuf[WORDS];            // куди

static DMA_HandleTypeDef hdma;            // "паспорт" DMA-потоку для бібліотеки HAL
static volatile bool dmaDone = false;     // volatile: змінюється в перериванні
static volatile bool dmaError = false;

// Обробник переривання DMA2 Stream0. Назва фіксована (з таблиці векторів).
// HAL сам розбирається, що сталося (готово / помилка), і викликає наші callback-и.
extern "C" void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma);
}

// Викликається з переривання, коли DMA скопіював усе. Тут - тільки прапорець:
// у перериванні не можна робити нічого довгого (наприклад, Serial.print).
static void onDmaComplete(DMA_HandleTypeDef*)
{
    dmaDone = true;
}

static void onDmaError(DMA_HandleTypeDef*)
{
    dmaError = true;
}

static void dmaInit()
{
    __HAL_RCC_DMA2_CLK_ENABLE();  // подати тактування на DMA2 (без цього він "мертвий")

    hdma.Instance = DMA2_Stream0;                  // який потік
    hdma.Init.Channel = DMA_CHANNEL_0;             // для пам'ять->пам'ять канал не важливий
    hdma.Init.Direction = DMA_MEMORY_TO_MEMORY;    // напрямок
    hdma.Init.PeriphInc = DMA_PINC_ENABLE;         // адреса джерела +4 після кожного слова
    hdma.Init.MemInc = DMA_MINC_ENABLE;            // адреса призначення +4 після кожного слова
    hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;  // елемент = 32 біти
    hdma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma.Init.Mode = DMA_NORMAL;                   // один раз (а не по колу)
    hdma.Init.Priority = DMA_PRIORITY_HIGH;
    // Пам'ять->пам'ять на F4 працює лише через внутрішній FIFO DMA (4 слова).
    // Burst INC4 = DMA переносить по 4 слова за один "захід" - так швидше.
    hdma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma.Init.MemBurst = DMA_MBURST_INC4;
    hdma.Init.PeriphBurst = DMA_PBURST_INC4;
    if (HAL_DMA_Init(&hdma) != HAL_OK) {
        Serial.println("HAL_DMA_Init failed");
    }

    hdma.XferCpltCallback = onDmaComplete;
    hdma.XferErrorCallback = onDmaError;

    // Дозволяємо переривання від цього потоку в контролері переривань (NVIC)
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

// Найпростіше копіювання: по байту.
static void __attribute__((noinline)) cpuCopyBytes(uint8_t* dst, const uint8_t* src, uint32_t n)
{
    volatile uint8_t* d = dst;
    for (uint32_t i = 0; i < n; i++) {
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
    Serial.printf(
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
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
    }  // почекати, поки відкриють Serial monitor (до 4 с)
    Serial.println("\nDMA demo 1: memory -> memory (STM32F411, DMA2)");
    dmaInit();
}

void loop()
{
    static uint32_t round = 0;
    round++;

    // Щоразу нові дані в джерелі - щоб перевірка "data OK" була чесною
    for (uint32_t i = 0; i < WORDS; i++) {
        srcBuf[i] = i * 2654435761u + round;
    }
    Serial.printf("\n===== Round %lu: copy %lu bytes =====\n", (unsigned long)round, (unsigned long)BYTES);

    // ---- 1) CPU по байту. CPU зайнятий 100% часу.
    memset(dstBuf, 0, BYTES);
    uint32_t t0 = micros();
    cpuCopyBytes((uint8_t*)dstBuf, (const uint8_t*)srcBuf, BYTES);
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
    dmaError = false;
    uint32_t work = 0;        // скільки "іншої роботи" CPU встиг зробити
    uint16_t left[10];        // знімки лічильника DMA: скільки слів ще залишилось
    int nLeft = 0;

    t0 = micros();
    // Завдання для DMA: звідки, куди, скільки ЕЛЕМЕНТІВ (тут слів по 4 байти)
    HAL_DMA_Start_IT(&hdma, (uint32_t)srcBuf, (uint32_t)dstBuf, WORDS);
    const uint32_t busy = micros() - t0;  // стільки CPU витратив на запуск

    // Поки DMA працює - CPU робить "інше". У реальній програмі тут була б
    // обробка кнопок, дисплей, обчислення... У нас - простий лічильник.
    while (!dmaDone && !dmaError) {
        work++;
        // NDTR - регістр DMA "скільки ще залишилось". Його зменшує сам DMA.
        const uint16_t now = __HAL_DMA_GET_COUNTER(&hdma);
        if (nLeft < 10 && (nLeft == 0 || left[nLeft - 1] - now >= WORDS / 8)) {
            left[nLeft++] = now;
        }
    }
    t = micros() - t0;

    if (dmaError) {
        Serial.println("3) DMA ERROR");
    }
    else {
        printResult("3) DMA", t, busy, dataOk());
        Serial.printf("   while DMA was copying, CPU did %lu loops of other work\n", (unsigned long)work);
        Serial.print("   DMA counter (words left), sampled by CPU:");
        for (int i = 0; i < nLeft; i++) {
            Serial.printf(" %u", left[i]);
        }
        Serial.println(" -> 0 (done)");
    }

    delay(3000);
}
