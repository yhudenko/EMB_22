// Навчальний приклад SPI: камера -> JPEG -> SD карта в SPI-режимі.
// SD підключена до пінів SDMMC, але карта підтримує SPI:
//   CLK  = GPIO14
//   MOSI = GPIO15 (CMD)
//   MISO = GPIO2  (D0)
//   CS   = GPIO13 (D3)
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "esp_camera.h"


#define SD_SCK 14
#define SD_MOSI 15
#define SD_MISO 2
#define SD_CS 13

// Піни камери AI-Thinker
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

SPIClass sdSpi(HSPI);  // окрема SPI-шина для SD
int frameNo = 0;

bool initCamera()
{
    camera_config_t c = {};
    // LEDC (ШІМ) генерує тактовий сигнал XCLK для камери; канал/таймер можна взяти будь-які вільні
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer = LEDC_TIMER_0;
    // Пини D0-D7: 8-бітна паралельна шина даних пікселів
    c.pin_d0 = Y2_GPIO_NUM;
    c.pin_d1 = Y3_GPIO_NUM;
    c.pin_d2 = Y4_GPIO_NUM;
    c.pin_d3 = Y5_GPIO_NUM;
    c.pin_d4 = Y6_GPIO_NUM;
    c.pin_d5 = Y7_GPIO_NUM;
    c.pin_d6 = Y8_GPIO_NUM;
    c.pin_d7 = Y9_GPIO_NUM;
    c.pin_xclk = XCLK_GPIO_NUM;     // тактовий сигнал від ESP32 до камери
    c.pin_pclk = PCLK_GPIO_NUM;     // піксельний такт від камери
    c.pin_vsync = VSYNC_GPIO_NUM;   // початок кадру
    c.pin_href = HREF_GPIO_NUM;     // валідний рядок
    c.pin_sccb_sda = SIOD_GPIO_NUM; // SCCB (аналог I2C) для налаштування сенсора
    c.pin_sccb_scl = SIOC_GPIO_NUM;
    c.pin_pwdn = PWDN_GPIO_NUM;     // живлення камери (вимкнення)
    c.pin_reset = RESET_GPIO_NUM;   // -1 = пін не використовується
    // Частота XCLK: 20 МГц стандарт; 10 МГц - стабільніше, але повільніше
    c.xclk_freq_hz = 20000000;
    // Формат: JPEG; RGB565 / GRAYSCALE / YUV422 - сирі пікселі, багато пам'яті
    c.pixel_format = PIXFORMAT_JPEG;
    // Роздільність: QQVGA(160x120), QVGA(320x240), VGA(640x480), SVGA(800x600), XGA, HD, UXGA(1600x1200)
    c.frame_size = FRAMESIZE_SVGA;
    // Якість JPEG: 0-63, менше число = краща якість і більший файл
    c.jpeg_quality = 12;
    // Кількість буферів кадру: 1 - мінімум; 2 - плавніше відео, але більше пам'яті
    c.fb_count = 1;
    // Де зберігати кадр: PSRAM (великий, є на ESP32-CAM) або DRAM (мало місця)
    c.fb_location = CAMERA_FB_IN_PSRAM;
    // WHEN_EMPTY - брати кадр, коли буфер вільний; LATEST - завжди найсвіжіший кадр
    c.grab_mode = CAMERA_GRAB_LATEST;
    return esp_camera_init(&c) == ESP_OK;
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    if (!initCamera()) {
        Serial.println("Camera init failed");
        return;
    }

    // SPI: SCK, MISO, MOSI, SS
    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    // 4 МГц - частота SPI; CS керується бібліотекою SD
    if (!SD.begin(SD_CS, sdSpi, 4000000)) {
        Serial.println("SD mount failed");
        return;
    }
    Serial.printf("SD size: %llu MB\n", SD.cardSize() / (1024 * 1024));
}

void loop()
{
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Capture failed");
        delay(1000);
        return;
    }

    char path[32];
    snprintf(path, sizeof(path), "/img_%04d.jpg", frameNo++);
    File f = SD.open(path, FILE_WRITE);
    if (f) {
        uint32_t t = millis();
        size_t n = f.write(fb->buf, fb->len);
        f.close();
        uint32_t dt = millis() - t;
        Serial.printf("%s: %u bytes, %u ms (%.1f KB/s)\n", path, (unsigned)n, dt, dt ? n / 1.024 / dt : 0.0);
    }
    else {
        Serial.println("File open failed");
    }
    esp_camera_fb_return(fb);
    delay(2000);
}
