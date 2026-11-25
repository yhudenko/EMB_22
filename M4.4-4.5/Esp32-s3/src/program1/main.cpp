// ESP32-S3 = SPI MASTER. Раз на секунду формує рядок "ms,seq,temp" і шле на
// STM32.
//
// Підключення:
//   ESP32 GPIO10 (CS)   -> STM32 PA4 (NSS)
//   ESP32 GPIO12 (SCK)  -> STM32 PA5 (SCK)
//   ESP32 GPIO13 (MISO) <- STM32 PA6 (MISO)
//   ESP32 GPIO11 (MOSI) -> STM32 PA7 (MOSI)
//   GND <-> GND (обов'язково)
#include <Arduino.h>
#include <SPI.h>

#include "spi_proto.h"

constexpr int PIN_CS = 10;
constexpr int PIN_SCK = 12;
constexpr int PIN_MISO = 13;
constexpr int PIN_MOSI = 11;

// Розібрана відповідь slave (див. формат у spi_proto.h).
struct Status {
    uint8_t lastSeq;
    uint8_t status;
    uint8_t badFrames;
    uint32_t totalBytes;
};

// Одна повнодуплексна транзакція: CS low -> 64 байти -> CS high.
// tx - що відправляємо, rx - сюди одночасно потрапляє те, що видав slave.
// SPI_MODE0 (CPOL=0, CPHA=0) має збігатися з налаштуванням slave. 1 МГц - з запасом для проводів.
// Невеликі паузи навколо CS дають slave час побачити спад/фронт NSS.
static void transfer(const uint8_t* tx, uint8_t* rx)
{
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    delayMicroseconds(20);
    SPI.transferBytes(tx, rx, FRAME_SIZE);
    delayMicroseconds(20);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
}

// Збирає кадр запиту: [magic][type][seq][len][payload][нулі][crc8]. Формат у spi_proto.h.
static void buildFrame(uint8_t* f, uint8_t type, uint8_t seq, const uint8_t* payload, size_t len)
{
    memset(f, 0, FRAME_SIZE);
    f[0] = MAGIC_REQ;
    f[1] = type;
    f[2] = seq;
    f[3] = (uint8_t)len;
    if (len) {
        memcpy(&f[4], payload, len);
    }
    f[FRAME_SIZE - 1] = crc8(f, FRAME_SIZE - 1);
}

// Перевіряє, що відповідь справжня (magic + CRC), і витягує поля.
// false = це не відповідь, а сміття (slave ще не був готовий).
static bool parseStatus(const uint8_t* r, Status& s)
{
    if (r[0] != MAGIC_RSP || crc8(r, FRAME_SIZE - 1) != r[FRAME_SIZE - 1]) {
        return false;
    }
    s.lastSeq = r[1];
    s.status = r[2];
    s.badFrames = r[3];
    // збираємо uint32 з 4 байтів, молодший першим (little-endian)
    s.totalBytes = r[4] | (r[5] << 8) | (r[6] << 16) | ((uint32_t)r[7] << 24);
    return true;
}

// Надсилає DATA, чекає поки STM32 запише на SD, потім читає статус через POLL.
static bool sendRecord(uint8_t seq, const uint8_t* payload, size_t len)
{
    uint8_t tx[FRAME_SIZE], rx[FRAME_SIZE];

    // До 3 спроб: якщо кадр зіпсувався або запис не вдався - шлемо знову з тим самим seq.
    for (int attempt = 1; attempt <= 3; attempt++) {
        buildFrame(tx, TYPE_DATA, seq, payload, len);
        transfer(tx, rx);  // відповідь на цей кадр ігноруємо (це старий статус)

        // Тепер питаємо статус: POLL-кадр нічого не записує, його відповідь - це результат DATA.
        buildFrame(tx, TYPE_POLL, 0, nullptr, 0);
        for (int poll = 0; poll < 15; poll++) {
            delay(20);  // час на запис в SD
            transfer(tx, rx);
            Status st;
            if (!parseStatus(rx, st)) {
                continue;  // slave ще зайнятий: MISO "порожній"
            }
            if (st.lastSeq == seq && st.status == ST_OK) {
                Serial.printf("seq=%u OK  total=%lu bytes  badFrames=%u (try %d)\n", seq, (unsigned long)st.totalBytes, st.badFrames, attempt);
                return true;
            }
            Serial.printf("seq=%u status: lastSeq=%u st=%u\n", seq, st.lastSeq, st.status);
            break;  // валідна, але не підтверджує наш кадр -> повтор DATA
        }
    }
    return false;
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);  // CS керуємо вручну
    delay(2000);                                 // дати STM32 час стартувати
    Serial.println("ESP32 SPI master ready");
}

void loop()
{
    static uint8_t seq = 0;  // номер кадру; зберігається між викликами loop()
    // "Дані" для прикладу: мілісекунди, номер, температура чипа. На практиці тут ваш датчик/файл.
    char line[PAYLOAD_MAX];
    int n = snprintf(line, sizeof(line), "%lu,%u,%.2f\n", millis(), seq, temperatureRead());

    if (!sendRecord(seq, (const uint8_t*)line, n)) {
        Serial.printf("seq=%u FAILED after retries\n", seq);
    }
    seq++;  // uint8_t: природно обертається 255 -> 0
    delay(1000);
}
