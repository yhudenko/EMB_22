# ESP32-S3: DMA (GDMA)

## Чи прив'язані DMA-канали до периферії, як у STM32?

**Ні.** На відміну від STM32F4 (фіксована таблиця Stream/Channel), в ESP32-S3 використовується
**GDMA (General DMA)** — канали загального призначення, які можна під'єднати до будь-якої
периферії з підтримкою DMA. Це ближче до DMAMUX у STM32G4/H7.

- 5 TX + 5 RX каналів.
- Будь-який канал може бути під'єднаний до будь-якої DMA-периферії через
  `GDMA_PERI_IN_SEL_CHn` / `GDMA_PERI_OUT_SEL_CHn`.
- Одна периферія в одному напрямку (TX/RX) — лише до одного каналу одночасно.
- Пріоритет каналу: 0–9.

## Використання в ESP-IDF / Arduino

- SPI: `spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);` — на S3 підтримується лише `SPI_DMA_CH_AUTO`.
- Ручне керування каналами: `gdma_new_channel()` + `gdma_connect()`.

## Периферія з підтримкою GDMA (ID для `PERI_SEL`)

| ID | Периферія |
|---|---|
| 0 | SPI2 |
| 1 | SPI3 |
| 2 | UHCI0 (UART через DMA) |
| 3 | I2S0 |
| 4 | I2S1 |
| 5 | LCD_CAM |
| 6 | AES |
| 7 | SHA |
| 8 | ADC (program5, continuous mode) |
| 9 | RMT |

Без DMA: I2C, LEDC, TWAI, UART напряму (для UART з DMA використовується UHCI0).

> Перевірити значення можна в розділі GDMA:
> [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf).
