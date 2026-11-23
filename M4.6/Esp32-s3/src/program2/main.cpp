// ============================================================================
// DMA ДЕМО 2: UART                       (ESP32-S3)
// ============================================================================
//
// ПІДКЛЮЧЕННЯ
//   Перемичка (дріт) GPIO17 -> GPIO18. Тобто UART1 TX з'єднаний зі своїм же RX:
//   усе, що плата відправляє, вона ж і приймає ("loopback").
//   Вивід - Serial0 (115200).
//
// ЯК UART ПРАЦЮЄ БЕЗ DMA
//   CPU кладе байти в апаратний FIFO UART (128 байтів), UART їх по одному
//   виштовхує на лінію. 2000 байтів на 115200 бод = ~174 мс. Якщо CPU сам
//   підкладає байти і чекає - він зайнятий усі ці 174 мс.
//   (Стандартний Serial1 / uart_write_bytes робить це через переривання і
//   буфер у RAM - це НЕ DMA, але теж розвантажує CPU.)
//
// ЯК З DMA НА ESP32-S3
//   UART не під'єднується до DMA напряму. Між ними стоїть блок UHCI
//   ("перехідник" UART <-> DMA):
//
//       пам'ять --GDMA TX--> UHCI --> UART1 TX ---дріт---> UART1 RX --> UHCI --GDMA RX--> пам'ять
//
//   CPU лише каже GDMA: "ось буфер txBuf на N байтів" - і вільний.
//   Прийом: GDMA складає байти в rxBuf; коли на лінії пауза (idle) - UHCI
//   вважає повідомлення завершеним і GDMA викликає наш callback.
//
// ЧОМУ ТУТ "НИЗЬКИЙ РІВЕНЬ"
//   В Arduino-ESP32 2.x (ESP-IDF 4.4) немає готового драйвера "UART через DMA",
//   тому налаштовуємо UHCI (регістри UHCI0.*) і GDMA (функції gdma_*) вручну.
//   У ESP-IDF 5.5+ для цього з'явився готовий драйвер driver/uhci.h.
//
// ДЕСКРИПТОР DMA - "накладна" для вантажника:
//   { size: місткість буфера, length: скільки байтів справжні,
//     suc_eof: "це останній шматок", owner: хто зараз володіє (CPU чи DMA),
//     buffer: адреса буфера, next: наступний дескриптор або NULL }
// ============================================================================
#include <Arduino.h>

#include "driver/periph_ctrl.h"
#include "driver/uart.h"
#include "esp_private/gdma.h"
#include "hal/dma_types.h"
#include "hal/uart_ll.h"
#include "soc/uhci_struct.h"

constexpr int PIN_TX = 17;
constexpr int PIN_RX = 18;
constexpr uint32_t BAUD = 115200;

// Буфери і дескриптори мають бути у внутрішній RAM (static - саме там) і
// вирівняні на 4 байти. Один дескриптор описує максимум 4095 байтів.
alignas(4) static uint8_t txBuf[2048];
alignas(4) static uint8_t rxBuf[4092];
static dma_descriptor_t txDesc;
static dma_descriptor_t rxDesc;

static gdma_channel_handle_t txChan = nullptr;
static gdma_channel_handle_t rxChan = nullptr;

static volatile bool txDone = false;
static volatile bool rxDone = false;
static volatile uint32_t rxLen = 0;

// ---- Callback-и GDMA: викликаються з ПЕРЕРИВАННЯ. Лише прапорці!
// TX: DMA віддав в UART останній байт ("EOF" = end of frame)
static bool IRAM_ATTR onTxEof(gdma_channel_handle_t, gdma_event_data_t*, void*)
{
    txDone = true;
    return false;
}

// RX: UHCI побачив паузу на лінії -> кінець повідомлення. Скільки байтів
// прийнято - DMA сам записав у поле length дескриптора.
static bool IRAM_ATTR onRxEof(gdma_channel_handle_t, gdma_event_data_t* e, void*)
{
    const dma_descriptor_t* d = (const dma_descriptor_t*)e->rx_eof_desc_addr;
    rxLen = d->dw0.length;
    rxDone = true;
    return false;
}

static void uartDmaInit()
{
    // 1) UART1: лише параметри і піни. Драйвер (uart_driver_install) НЕ ставимо:
    //    байти UART-у тепер забирає/приносить UHCI, а не переривання драйвера.
    uart_config_t cfg = {};
    cfg.baud_rate = BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_APB;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, PIN_TX, PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // "Пауза" = лінія мовчить довше за 30 біт (~3 символи) -> кінець повідомлення
    uart_ll_set_rx_idle_thr(&UART1, 30);

    // 2) UHCI0: увімкнути тактування, скинути і налаштувати регістри напряму.
    //    UHCI за замовчуванням вміє "пакувати" дані (заголовок, CRC, SLIP-заміна
    //    символів 0xC0/0xDB). Нам потрібні байти "як є", тому все це вимикаємо.
    periph_module_enable(PERIPH_UHCI0_MODULE);
    periph_module_reset(PERIPH_UHCI0_MODULE);
    UHCI0.conf0.val = 0;               // усе "пакування" вимкнено
    UHCI0.conf0.clk_en = 1;
    UHCI0.conf0.uart1_ce = 1;          // UHCI працює з UART1
    UHCI0.conf0.uart_idle_eof_en = 1;  // кінець прийому = пауза на лінії
    UHCI0.conf1.val = 0;               // без контрольних сум і номерів пакетів
    UHCI0.escape_conf.val = 0;         // без SLIP-замін

    // 3) Два канали GDMA: TX (пам'ять -> UHCI) і RX (UHCI -> пам'ять)
    gdma_channel_alloc_config_t txAlloc = {};
    txAlloc.direction = GDMA_CHANNEL_DIRECTION_TX;
    ESP_ERROR_CHECK(gdma_new_channel(&txAlloc, &txChan));
    gdma_connect(txChan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_UHCI, 0));

    gdma_channel_alloc_config_t rxAlloc = {};
    rxAlloc.direction = GDMA_CHANNEL_DIRECTION_RX;
    ESP_ERROR_CHECK(gdma_new_channel(&rxAlloc, &rxChan));
    gdma_connect(rxChan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_UHCI, 0));

    // owner_check: DMA бере лише дескриптори з owner = DMA.
    // auto_update_desc: після відправки DMA сам ставить owner = CPU ("повертає накладну").
    gdma_strategy_config_t strategy = {};
    strategy.owner_check = true;
    strategy.auto_update_desc = true;
    gdma_apply_strategy(txChan, &strategy);
    gdma_apply_strategy(rxChan, &strategy);

    gdma_tx_event_callbacks_t txCbs = {};
    txCbs.on_trans_eof = onTxEof;
    gdma_register_tx_event_callbacks(txChan, &txCbs, nullptr);
    gdma_rx_event_callbacks_t rxCbs = {};
    rxCbs.on_recv_eof = onRxEof;
    gdma_register_rx_event_callbacks(rxChan, &rxCbs, nullptr);
}

// "Озброїти" прийом: дати GDMA RX порожній буфер
static void startRx()
{
    gdma_stop(rxChan);
    gdma_reset(rxChan);
    memset(rxBuf, 0, sizeof(rxBuf));
    rxDone = false;
    rxLen = 0;

    rxDesc.dw0.size = sizeof(rxBuf);  // скільки місця є
    rxDesc.dw0.length = 0;            // скільки заповнено (запише DMA)
    rxDesc.dw0.suc_eof = 0;
    rxDesc.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    rxDesc.buffer = rxBuf;
    rxDesc.next = nullptr;
    gdma_start(rxChan, (intptr_t)&rxDesc);
}

// Запустити відправку len байтів з txBuf
static void startTx(size_t len)
{
    txDone = false;
    txDesc.dw0.size = (len + 3) & ~3u;  // місткість, вирівняна на 4
    txDesc.dw0.length = len;            // скільки відправити
    txDesc.dw0.suc_eof = 1;             // це весь пакет
    txDesc.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    txDesc.buffer = txBuf;
    txDesc.next = nullptr;
    gdma_start(txChan, (intptr_t)&txDesc);
}

// Повідомлення: багато рядків тексту, разом ~2 КБ
static size_t buildMessage(uint32_t round)
{
    size_t len = 0;
    int line = 1;
    while (len + 80 < sizeof(txBuf)) {
        len += snprintf((char*)txBuf + len, sizeof(txBuf) - len, "round %lu, line %02d: the quick brown fox jumps over the lazy dog\r\n", (unsigned long)round, line++);
    }
    return len;
}

void setup()
{
    Serial0.begin(115200);
    delay(500);
    Serial0.println("\nDMA demo 2: UART + DMA (ESP32-S3: GDMA + UHCI + UART1, jumper GPIO17 -> GPIO18)");
    uartDmaInit();
}

void loop()
{
    static uint32_t round = 0;
    round++;

    const size_t len = buildMessage(round);
    const uint32_t wireMs = (uint32_t)((uint64_t)len * 10 * 1000 / BAUD);  // 10 біт на байт
    Serial0.printf("\n===== Round %lu: send %u bytes at %lu baud =====\n", (unsigned long)round, (unsigned)len, (unsigned long)BAUD);
    Serial0.printf("Without DMA the CPU would be busy feeding the UART for ~%lu ms.\n", (unsigned long)wireMs);

    startRx();

    uint32_t work = 0;
    const uint32_t t0 = micros();
    startTx(len);
    const uint32_t busy = micros() - t0;  // стільки CPU витратив на запуск

    // Поки DMA відправляє і приймає - CPU вільний і рахує "іншу роботу"
    while (!txDone && micros() - t0 < 2000000) {
        work++;
    }
    const uint32_t txUs = micros() - t0;
    while (!rxDone && micros() - t0 < 2000000) {
        work++;
    }
    const uint32_t rxUs = micros() - t0;

    Serial0.printf("TX by DMA: CPU busy %lu us, DMA finished after %lu us\n", (unsigned long)busy, (unsigned long)txUs);
    if (!rxDone) {
        Serial0.println("RX: nothing received! Is the jumper GPIO17 -> GPIO18 installed?");
    }
    else {
        const bool same = rxLen == len && memcmp(rxBuf, txBuf, len) == 0;
        Serial0.printf("RX by DMA: %lu bytes after %lu us, %s\n", (unsigned long)rxLen, (unsigned long)rxUs, same ? "identical to TX" : "DIFFERENT from TX!");
        const char* nl = (const char*)memchr(rxBuf, '\n', rxLen);
        const int firstLine = nl && nl > (const char*)rxBuf ? (int)(nl - (const char*)rxBuf - 1) : 0;  // без \r\n
        Serial0.printf("first line: \"%.*s\"\n", firstLine, (const char*)rxBuf);
    }
    Serial0.printf("Meanwhile CPU was free and did %lu loops of other work.\n", (unsigned long)work);

    delay(3000);
}
