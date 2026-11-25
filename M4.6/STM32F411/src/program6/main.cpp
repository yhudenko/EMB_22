// ============================================================================
// ДЕМО 6: ПРЯМИЙ ЗАПИС І ЧИТАННЯ РЕГІСТРІВ          (STM32F411 Black Pill)
// ============================================================================
//
// ЩО ТАКЕ РЕГІСТР
//   Уся периферія (GPIO, RCC, DMA, UART ...) керується регістрами - 32-бітними
//   "комірками" за ФІКСОВАНИМИ адресами (memory-mapped I/O). Для CPU це звичайна
//   пам'ять: записав число за адресою - змінився стан ніжки / запустився DMA;
//   прочитав - дізнався стан кнопки / прапорці.
//   Адреси й значення бітів - у Reference Manual RM0383.
//
// ТРИ СПОСОБИ ДОСТУПУ (від низького рівня до високого)
//   1) "Сирий" вказівник:  *(volatile uint32_t*)0x40020814 = ...
//   2) CMSIS-структура:    GPIOC->ODR = ...   (та сама адреса, але з іменами)
//   3) HAL / Arduino:      digitalWrite(PC13, LOW) - всередині все одно пише
//                          в ті самі регістри, тільки з перевірками й накладними.
//   volatile - обов'язково: каже компілятору "значення може змінитись саме
//   (апаратно), не кешуй його і не викидай записи".
//
// ЩО РОБИТЬ ПРОГРАМА
//   1. Читає read-only регістри: ID чіпа, розмір Flash, унікальний 96-бітний UID.
//   2. Вмикає тактування і налаштовує GPIO: PC13 (LED) - вихід, PA0 (KEY) - вхід
//      з підтяжкою. Друкує регістри ДО і ПІСЛЯ запису.
//   3. Порівнює швидкість: digitalWrite() vs ODR ^= vs BSRR (лічильник тактів DWT).
//   4. Копіює буфер DMA2 пам'ять -> пам'ять БЕЗ HAL - тільки регістрами.
//   5. loop(): LED блимає (ODR ^=), поки KEY натиснута - світить постійно (BSRR).
//      Кожне натискання друкує IDR і повторює DMA-демо.
//
// ПІДКЛЮЧЕННЯ: нічого (LED PC13 і кнопка KEY PA0 є на платі).
// Вивід - Serial monitor через USB-C (115200).
//
// НА ЩО ЗВЕРНУТИ УВАГУ
//   - Read-modify-write: щоб змінити кілька бітів, читаємо регістр, чистимо
//     маску, ставимо нові біти, записуємо. НІКОЛИ не пишіть GPIOA->MODER = x
//     цілком: на PA11/PA12 - USB, на PA13/PA14 - SWD (ST-Link). Зламаєте зв'язок.
//   - BSRR атомарний: запис 1 у біт n ставить пін n в 1, у біт n+16 - в 0.
//     Нулі ігноруються, тому читати регістр не треба і переривання не завадить.
//   - Прапорці DMA (LISR) лише читаються; скидаються записом 1 у LIFCR
//     ("write 1 to clear").
//   - Без тактування (RCC->AHB1ENR) периферія "мертва": записи ігноруються.
// ============================================================================
#include <Arduino.h>

constexpr uint32_t LED_PIN = 13;  // PC13, активний LOW (0 = світить)
constexpr uint32_t BTN_PIN = 0;   // PA0, кнопка KEY замикає на GND

// Друк регістра: ім'я, адреса, значення. #r перетворює вираз на рядок.
#define PRINT_REG(r) printReg(#r, (uint32_t)&(r), (r))

static void printReg(const char* name, uint32_t addr, uint32_t value)
{
    Serial.printf("  %-16s @0x%08lX = 0x%08lX\n", name, (unsigned long)addr, (unsigned long)value);
}

// 2-бітне поле піна в MODER / PUPDR / OSPEEDR
static uint32_t field2(uint32_t reg, uint32_t pin)
{
    return (reg >> (pin * 2)) & 0x3u;
}

static inline void ledOn() { GPIOC->BSRR = 1u << (LED_PIN + 16); }  // reset -> 0 -> світить
static inline void ledOff() { GPIOC->BSRR = 1u << LED_PIN; }        // set   -> 1 -> гасне
static inline bool keyPressed() { return (GPIOA->IDR & (1u << BTN_PIN)) == 0; }

// ---------------------------------------------------------------------------
// 1. Read-only регістри: ідентифікація чіпа
// ---------------------------------------------------------------------------
static void demoIds()
{
    Serial.println("\n--- 1. Read-only registers: chip ID ---");

    // Той самий регістр DBGMCU_IDCODE двома способами
    const uint32_t idRaw = *(volatile uint32_t*)0xE0042000;  // адреса з RM0383
    const uint32_t idCmsis = DBGMCU->IDCODE;                  // CMSIS-структура
    Serial.printf("  DBGMCU_IDCODE: raw pointer = 0x%08lX, CMSIS = 0x%08lX\n",
                  (unsigned long)idRaw, (unsigned long)idCmsis);
    Serial.printf("  DEV_ID = 0x%03lX (0x431 = STM32F411), REV_ID = 0x%04lX\n",
                  (unsigned long)(idRaw & 0xFFF), (unsigned long)(idRaw >> 16));

    // Розмір Flash - 16-бітне значення у системній пам'яті
    const uint16_t flashKb = *(volatile uint16_t*)FLASHSIZE_BASE;
    Serial.printf("  Flash size     @0x%08lX = %u KB\n", (unsigned long)FLASHSIZE_BASE, flashKb);

    // Унікальний ID - три 32-бітні слова, у кожного чіпа свій
    const volatile uint32_t* uid = (const volatile uint32_t*)UID_BASE;
    Serial.printf("  UID            @0x%08lX = %08lX-%08lX-%08lX\n", (unsigned long)UID_BASE,
                  (unsigned long)uid[2], (unsigned long)uid[1], (unsigned long)uid[0]);
}

// ---------------------------------------------------------------------------
// 2. GPIO через регістри
// ---------------------------------------------------------------------------
static void demoGpioInit()
{
    Serial.println("\n--- 2. GPIO through registers ---");

    // Крок 1: тактування портів A і C (шина AHB1)
    Serial.println("  Enable clock for GPIOA, GPIOC:");
    PRINT_REG(RCC->AHB1ENR);
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;  // читання назад = коротка пауза, поки тактування "дійде" (errata)
    PRINT_REG(RCC->AHB1ENR);

    // Крок 2: PC13 - вихід push-pull. MODER: 2 біти на пін, 00=вхід 01=вихід 10=AF 11=аналог
    Serial.printf("  PC13 MODER field before = %lu\n", (unsigned long)field2(GPIOC->MODER, LED_PIN));
    PRINT_REG(GPIOC->MODER);
    ledOff();  // спершу задаємо рівень, потім вмикаємо вихід - щоб LED не "блимнув"
    // Read-modify-write поля MODER13 (біти 27:26), решта пінів порту C не змінюється:
    //   LED_PIN * 2          = 26          - поле піна 13 починається з біта 26
    //   0x3u << 26           = 0b11 << 26  - маска поля (біти 27 і 26 = 1)
    //   ~(маска)             - усі біти 1, крім 27:26
    //   MODER & ~(маска)     - читаємо регістр і ОБНУЛЯЄМО поле: 27:26 = 00
    //   | (0x1u << 26)       - записуємо в поле 01 (output): біт 26 = 1
    //   GPIOC->MODER = ...   - записуємо результат назад у регістр
    // Приклад: було 0x00000000 -> стало 0x04000000 (біт 26 = 1).
    GPIOC->MODER = (GPIOC->MODER & ~(0x3u << (LED_PIN * 2))) | (0x1u << (LED_PIN * 2));
    GPIOC->OTYPER &= ~(1u << LED_PIN);               // 0 = push-pull
    GPIOC->OSPEEDR &= ~(0x3u << (LED_PIN * 2));      // 00 = low speed
    GPIOC->PUPDR &= ~(0x3u << (LED_PIN * 2));        // 00 = без підтяжки
    PRINT_REG(GPIOC->MODER);
    Serial.printf("  PC13 MODER field after  = %lu (01 = output)\n", (unsigned long)field2(GPIOC->MODER, LED_PIN));

    // Крок 3: PA0 - вхід з підтяжкою до 3V3. PUPDR: 00=немає 01=pull-up 10=pull-down
    // Тільки read-modify-write! Решта бітів GPIOA - це USB і SWD.
    PRINT_REG(GPIOA->MODER);
    PRINT_REG(GPIOA->PUPDR);
    GPIOA->MODER &= ~(0x3u << (BTN_PIN * 2));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(0x3u << (BTN_PIN * 2))) | (0x1u << (BTN_PIN * 2));
    PRINT_REG(GPIOA->PUPDR);
    Serial.printf("  PA0 MODER=%lu (input), PUPDR=%lu (pull-up)\n",
                  (unsigned long)field2(GPIOA->MODER, BTN_PIN), (unsigned long)field2(GPIOA->PUPDR, BTN_PIN));

    // Крок 4: читаємо вхід (IDR) і вихід (ODR)
    PRINT_REG(GPIOA->IDR);
    Serial.printf("  KEY (PA0) = %s\n", keyPressed() ? "pressed (0)" : "released (1)");
    ledOn();
    Serial.printf("  ledOn():  GPIOC->ODR bit13 = %lu (LED on)\n", (unsigned long)((GPIOC->ODR >> LED_PIN) & 1));
    ledOff();
    Serial.printf("  ledOff(): GPIOC->ODR bit13 = %lu (LED off)\n", (unsigned long)((GPIOC->ODR >> LED_PIN) & 1));
}

// ---------------------------------------------------------------------------
// 3. Швидкість: Arduino vs регістри. Такти рахує DWT->CYCCNT (теж регістр ядра).
// ---------------------------------------------------------------------------
static void demoSpeed()
{
    Serial.println("\n--- 3. Toggle speed (1000 on/off pairs, DWT cycle counter) ---");

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // увімкнути блок трасування
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // запустити лічильник тактів

    constexpr uint32_t N = 1000;
    constexpr uint32_t mask = 1u << LED_PIN;
    uint32_t c0;

    c0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < N; i++) {
        digitalWrite(PC13, LOW);
        digitalWrite(PC13, HIGH);
    }
    const uint32_t cArduino = DWT->CYCCNT - c0;

    c0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < N; i++) {
        GPIOC->ODR ^= mask;  // read-modify-write: читання + XOR + запис
        GPIOC->ODR ^= mask;
    }
    const uint32_t cOdr = DWT->CYCCNT - c0;

    c0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < N; i++) {
        GPIOC->BSRR = mask << 16;  // один запис, без читання
        GPIOC->BSRR = mask;
    }
    const uint32_t cBsrr = DWT->CYCCNT - c0;
    ledOff();

    const uint32_t mhz = SystemCoreClock / 1000000;
    auto row = [&](const char* name, uint32_t cycles) {
        Serial.printf("  %-22s %6lu cycles/pair = %5lu ns\n", name, (unsigned long)(cycles / N),
                      (unsigned long)(cycles * 1000 / mhz / N));
    };
    Serial.printf("  CPU clock = %lu MHz\n", (unsigned long)mhz);
    row("digitalWrite() x2", cArduino);
    row("GPIOC->ODR ^= x2", cOdr);
    row("GPIOC->BSRR = x2", cBsrr);
}

// ---------------------------------------------------------------------------
// 4. DMA2 Stream1 пам'ять -> пам'ять, тільки регістри (порівняйте з program1/HAL)
// ---------------------------------------------------------------------------
constexpr uint32_t DMA_WORDS = 1024;
static uint32_t dmaSrc[DMA_WORDS];
static uint32_t dmaDst[DMA_WORDS];

static void dumpDma(const char* when)
{
    DMA_Stream_TypeDef* const s = DMA2_Stream1;
    Serial.printf("  %-10s CR=0x%08lX NDTR=%4lu FCR=0x%02lX LISR=0x%08lX\n", when, (unsigned long)s->CR,
                  (unsigned long)s->NDTR, (unsigned long)s->FCR, (unsigned long)DMA2->LISR);
}

static void demoDma()
{
    Serial.println("\n--- 4. DMA2 Stream1 memory -> memory via registers ---");
    DMA_Stream_TypeDef* const s = DMA2_Stream1;
    // Прапорці потоку 1 у LISR/LIFCR - біти 6..11 (потік 0 - біти 0..5)
    constexpr uint32_t allFlags1 =
        DMA_LIFCR_CTCIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CFEIF1;

    static uint32_t round = 0;
    round++;
    for (uint32_t i = 0; i < DMA_WORDS; i++) {
        dmaSrc[i] = i * 2654435761u + round;
    }
    memset(dmaDst, 0, sizeof(dmaDst));

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;  // тактування DMA2
    (void)RCC->AHB1ENR;

    // Налаштовувати потік можна тільки коли він вимкнений (EN=0)
    s->CR &= ~DMA_SxCR_EN;
    while (s->CR & DMA_SxCR_EN) {
    }
    DMA2->LIFCR = allFlags1;  // write 1 to clear: скинути старі прапорці

    s->PAR = (uint32_t)dmaSrc;   // для mem->mem: "периферія" = джерело
    s->M0AR = (uint32_t)dmaDst;  // пам'ять = призначення
    s->NDTR = DMA_WORDS;         // скільки елементів
    s->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;  // mem->mem потребує FIFO; поріг = повний (4 слова)
    s->CR = DMA_SxCR_DIR_1                     // DIR=10: пам'ять -> пам'ять
            | DMA_SxCR_PINC | DMA_SxCR_MINC    // обидві адреси +4 після кожного слова
            | DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1  // розмір елемента = 32 біти
            | DMA_SxCR_PL_1;                   // пріоритет high; CHSEL=0
    dumpDma("configured");

    const uint32_t c0 = DWT->CYCCNT;
    s->CR |= DMA_SxCR_EN;                   // СТАРТ
    const uint16_t ndtrJustAfter = s->NDTR;  // DMA вже працює, лічильник зменшується сам
    uint32_t spins = 0;
    while (!(DMA2->LISR & (DMA_LISR_TCIF1 | DMA_LISR_TEIF1))) {
        spins++;  // CPU вільний - тут могла б бути корисна робота
    }
    const uint32_t cycles = DWT->CYCCNT - c0;

    Serial.printf("  NDTR right after EN = %u (of %lu)\n", ndtrJustAfter, (unsigned long)DMA_WORDS);
    dumpDma("finished");  // EN скинувся сам, NDTR=0, у LISR стоїть TCIF1 (біт 11)
    const bool error = DMA2->LISR & DMA_LISR_TEIF1;
    DMA2->LIFCR = allFlags1;
    dumpDma("flags clr");

    Serial.printf("  %lu bytes in %lu cycles, CPU polled %lu times, data %s\n",
                  (unsigned long)sizeof(dmaDst), (unsigned long)cycles, (unsigned long)spins,
                  error ? "TRANSFER ERROR!" : (memcmp(dmaSrc, dmaDst, sizeof(dmaDst)) == 0 ? "OK" : "WRONG!"));
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
    }  // почекати, поки відкриють Serial monitor (до 4 с)
    Serial.println("\nDemo 6: direct register read/write (STM32F411)");

    demoIds();
    demoGpioInit();
    demoSpeed();
    demoDma();

    Serial.println("\nLED blinks via ODR ^=. Hold KEY (PA0): LED on via BSRR, DMA demo repeats.");
}

void loop()
{
    static bool lastPressed = false;
    static uint32_t lastBlink = 0;

    const bool pressed = keyPressed();
    if (pressed != lastPressed) {
        lastPressed = pressed;
        Serial.printf("\nKEY %s: GPIOA->IDR = 0x%04lX (bit0 = %lu)\n", pressed ? "pressed" : "released",
                      (unsigned long)(GPIOA->IDR & 0xFFFF), (unsigned long)(GPIOA->IDR & 1));
        if (pressed) {
            ledOn();
            demoDma();
        }
        delay(30);  // антибрязкіт
    }

    if (!pressed && millis() - lastBlink >= 500) {
        lastBlink = millis();
        GPIOC->ODR ^= 1u << LED_PIN;
    }
}
