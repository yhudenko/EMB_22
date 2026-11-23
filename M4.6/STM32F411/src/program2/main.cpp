// ============================================================================
// DMA ДЕМО 2: UART                       (STM32F411 Black Pill)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ
//   Перемичка (дріт) PA2 -> PA3. Тобто USART2 TX з'єднаний зі своїм же RX:
//   усе, що плата відправляє, вона ж і приймає ("loopback").
//   Вивід - Serial monitor через USB-C (115200).
//
// ДИВІТЬСЯ НА СВІТЛОДІОД PC13
//   Він швидко мигає ("серцебиття"), але лише тоді, коли CPU має час викликати
//   функцію heartbeat(). Відправка ~4 КБ на 115200 бод триває ~0.35 с:
//     A) без DMA (HAL_UART_Transmit)      -> CPU зайнятий, світлодіод ЗАВМИРАЄ
//     B) з DMA   (HAL_UART_Transmit_DMA)  -> CPU вільний, світлодіод МИГАЄ далі
//   Порада: зменшіть BAUD до 9600 - пауза стане ~4 с, і різницю видно одразу.
//
// ЯК UART ПРАЦЮЄ БЕЗ DMA
//   UART має регістр даних на 1 байт. CPU кладе байт, чекає ~87 мкс (115200 бод),
//   поки той вийде, кладе наступний... 4000 байтів = 4000 разів чекати.
// ЯК З DMA
//   CPU каже DMA: "ось буфер на 4000 байтів - віддавай в UART". Щойно UART
//   готовий прийняти наступний байт, він сам "смикає" DMA (запит DMA request),
//   і DMA підкладає байт. CPU про це навіть не знає. В кінці - callback.
//
// ПРИЙОМ ТЕЖ ЧЕРЕЗ DMA (в обох варіантах A і B)
//   HAL_UARTEx_ReceiveToIdle_DMA: DMA складає байти в rxBuf, а UART помічає
//   паузу на лінії (IDLE) = "повідомлення закінчилось" і викликає callback з
//   кількістю прийнятих байтів. Зручно, коли довжина повідомлення наперед невідома.
//   Зверніть увагу: навіть у варіанті A прийом іде паралельно з відправкою -
//   саме тому, що приймає DMA, а CPU в цей час зайнятий відправкою.
//
// РЕСУРСИ STM32F411 (таблиця "DMA1 request mapping" у Reference Manual):
//   USART2_TX = DMA1 Stream6 Channel4,  USART2_RX = DMA1 Stream5 Channel4.
// ============================================================================
#include <Arduino.h>

constexpr uint32_t BAUD = 115200;
constexpr uint32_t LED = PC13;  // світиться при LOW

static char txBuf[4000];
static uint8_t rxBuf[4200];  // трохи більший за повідомлення

static UART_HandleTypeDef huart2;
static DMA_HandleTypeDef hdmaTx;
static DMA_HandleTypeDef hdmaRx;

// Прапорці, які виставляються з переривань (callback-ів)
static volatile bool txDone = false;
static volatile bool rxDone = false;
static volatile uint16_t rxLen = 0;
static volatile bool uartError = false;

// ---- Обробники переривань: передаємо керування HAL, а він викличе callback-и
extern "C" void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);  // потрібен для IDLE і кінця передачі
}

extern "C" void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaRx);
}

extern "C" void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaTx);
}

// ---- Callback-и HAL (імена фіксовані, HAL їх шукає сам)
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* h)
{
    if (h->Instance == USART2) {
        txDone = true;  // DMA віддав в UART останній байт
    }
}

// Викликається, коли на лінії пауза (IDLE) або буфер заповнено. size = скільки прийнято.
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* h, uint16_t size)
{
    if (h->Instance == USART2) {
        rxLen = size;
        rxDone = true;
    }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* h)
{
    if (h->Instance == USART2) {
        uartError = true;
    }
}

// "Серцебиття": перемикає LED кожні 50 мс, але тільки якщо CPU встигає її викликати.
static void heartbeat()
{
    static uint32_t last = 0;
    if (millis() - last >= 50) {
        last = millis();
        digitalToggle(LED);
    }
}

static void uartInit()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // PA2 = TX, PA3 = RX: "альтернативна функція" AF7 = USART2
    GPIO_InitTypeDef g = {};
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;  // лінія UART у спокої = 1
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    // DMA для відправки: пам'ять (txBuf) -> периферія (регістр даних UART)
    hdmaTx.Instance = DMA1_Stream6;
    hdmaTx.Init.Channel = DMA_CHANNEL_4;
    hdmaTx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdmaTx.Init.PeriphInc = DMA_PINC_DISABLE;  // регістр UART завжди той самий
    hdmaTx.Init.MemInc = DMA_MINC_ENABLE;      // а по буферу йдемо вперед
    hdmaTx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdmaTx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdmaTx.Init.Mode = DMA_NORMAL;
    hdmaTx.Init.Priority = DMA_PRIORITY_LOW;
    hdmaTx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdmaTx);
    __HAL_LINKDMA(&huart2, hdmatx, hdmaTx);  // "прив'язати" цей DMA до UART2

    // DMA для прийому: периферія (регістр даних UART) -> пам'ять (rxBuf)
    hdmaRx.Instance = DMA1_Stream5;
    hdmaRx.Init.Channel = DMA_CHANNEL_4;
    hdmaRx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdmaRx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdmaRx.Init.MemInc = DMA_MINC_ENABLE;
    hdmaRx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdmaRx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdmaRx.Init.Mode = DMA_NORMAL;
    hdmaRx.Init.Priority = DMA_PRIORITY_HIGH;  // прийом важливіший: байт не можна загубити
    hdmaRx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdmaRx);
    __HAL_LINKDMA(&huart2, hdmarx, hdmaRx);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = BAUD;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

// "Озброїти" прийом: DMA чекає байти і складатиме їх у rxBuf
static void startRx()
{
    HAL_UART_AbortReceive(&huart2);  // на випадок, якщо попередній прийом не завершився
    memset(rxBuf, 0, sizeof(rxBuf));
    rxDone = false;
    rxLen = 0;
    uartError = false;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rxBuf, sizeof(rxBuf));
    // HAL за замовчуванням ще й повідомляє "половину буфера заповнено" - нам не треба
    __HAL_DMA_DISABLE_IT(&hdmaRx, DMA_IT_HT);
}

// Чекаємо кінця прийому. Поки чекаємо - CPU вільний, тому LED мигає.
static bool waitRx(uint32_t timeoutMs)
{
    const uint32_t t0 = millis();
    while (!rxDone) {
        heartbeat();
        if (millis() - t0 > timeoutMs) {
            return false;
        }
    }
    return true;
}

// Повідомлення: багато рядків тексту, разом ~4 КБ
static size_t buildMessage(uint32_t round)
{
    size_t len = 0;
    int line = 1;
    while (len + 80 < sizeof(txBuf)) {
        len += snprintf(txBuf + len, sizeof(txBuf) - len, "round %lu, line %02d: the quick brown fox jumps over the lazy dog\r\n", (unsigned long)round, line++);
    }
    return len;
}

// Перевірити прийняте і надрукувати перший рядок як доказ
static void checkReceived(size_t len)
{
    if (!waitRx(1000)) {
        Serial.println("   RX: nothing received! Is the jumper PA2 -> PA3 installed?");
        return;
    }
    const bool same = rxLen == len && memcmp(rxBuf, txBuf, len) == 0;
    Serial.printf("   RX (by DMA): %u bytes, %s%s\n", rxLen, same ? "identical to TX" : "DIFFERENT from TX!", uartError ? " (UART error)" : "");
    const char* nl = (const char*)memchr(rxBuf, '\n', rxLen);
    const int firstLine = nl && nl > (const char*)rxBuf ? (int)(nl - (const char*)rxBuf - 1) : 0;  // без \r\n
    Serial.printf("   first line: \"%.*s\"\n", firstLine, (const char*)rxBuf);
}

void setup()
{
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
        heartbeat();
    }
    Serial.println("\nDMA demo 2: UART (USART2 PA2 -> PA3 loopback). Watch the LED on PC13!");
    uartInit();
}

void loop()
{
    heartbeat();  // у "вільний" час LED мигає

    static uint32_t lastRun = 0;
    static uint32_t round = 0;
    if (millis() - lastRun < 4000) {
        return;
    }
    round++;

    const size_t len = buildMessage(round);
    const uint32_t expectUs = (uint32_t)((uint64_t)len * 10 * 1000000 / BAUD);  // 10 біт на байт
    Serial.printf("\n===== Round %lu: send %u bytes at %lu baud (~%lu ms on the wire) =====\n", (unsigned long)round, (unsigned)len, (unsigned long)BAUD, (unsigned long)(expectUs / 1000));

    // ---- A) Відправка БЕЗ DMA: функція повертається лише коли все відправлено
    Serial.println("A) HAL_UART_Transmit (no DMA)      -> LED freezes");
    Serial.flush();
    startRx();
    uint32_t t0 = micros();
    HAL_UART_Transmit(&huart2, (uint8_t*)txBuf, len, 5000);
    uint32_t busy = micros() - t0;
    Serial.printf("   TX: %lu us, CPU busy %lu us (100%%), other work done: 0\n", (unsigned long)busy, (unsigned long)busy);
    checkReceived(len);

    // ---- B) Відправка З DMA: функція лише запускає DMA і одразу повертається
    Serial.println("B) HAL_UART_Transmit_DMA           -> LED keeps blinking");
    Serial.flush();
    startRx();
    txDone = false;
    uint32_t work = 0;
    t0 = micros();
    HAL_UART_Transmit_DMA(&huart2, (uint8_t*)txBuf, len);
    busy = micros() - t0;
    while (!txDone) {  // поки DMA відправляє - CPU робить "інше"
        work++;
        heartbeat();
    }
    const uint32_t total = micros() - t0;
    Serial.printf("   TX: %lu us, CPU busy %lu us (%lu%%), other work done: %lu loops\n", (unsigned long)total, (unsigned long)busy, (unsigned long)(busy * 100 / total), (unsigned long)work);
    checkReceived(len);

    lastRun = millis();
}
