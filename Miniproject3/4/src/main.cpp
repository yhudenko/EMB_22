#include <Arduino.h>
#include <EncButton.h>

#define SERVO_PIN   4
#define BUZZER_PIN  5

#define ENC_CLK     6
#define ENC_DT      7
#define ENC_SW      8

// Сегменти a,b,c,d,e,f,g
const uint8_t segPins[7] = {
  9, 10, 11, 12, 13, 14, 15
};

// Канали PWM
const uint8_t SERVO_CH = 0;
const uint8_t BUZZER_CH = 1;

// Пароль
const uint8_t secretDigit = 4;

// EncButton 3.x
EncButton enc(ENC_CLK, ENC_DT, ENC_SW);

int currentDigit = 0;

// Спільний анод
const bool digits[10][7] = {
  {0,0,0,0,0,0,1}, //0
  {1,0,0,1,1,1,1}, //1
  {0,0,1,0,0,1,0}, //2
  {0,0,0,0,1,1,0}, //3
  {1,0,0,1,1,0,0}, //4
  {0,1,0,0,1,0,0}, //5
  {0,1,0,0,0,0,0}, //6
  {0,0,0,1,1,1,1}, //7
  {0,0,0,0,0,0,0}, //8
  {0,0,0,0,1,0,0}  //9
};

void showDigit(uint8_t n)
{
  for (uint8_t i = 0; i < 7; i++)
  {
    digitalWrite(segPins[i], digits[n][i]);
  }
}

void servoWriteAngle(int angle)
{
  angle = constrain(angle, 0, 180);

  uint32_t us = map(angle, 0, 180, 500, 2400); // 800 2200

  uint32_t duty = map(us, 500, 2400, 1638, 7864);

  ledcWrite(SERVO_CH, duty);
}

void buzzerOff()
{
  ledcWriteTone(BUZZER_CH, 0);
}

void beepError()
{
  ledcWriteTone(BUZZER_CH, 300);
  delay(300);
  buzzerOff();
}

void playSuccessMelody()
{
  const uint16_t notes[] = {
    523, 659, 784, 1047
  };

  for (uint8_t i = 0; i < 4; i++)
  {
    ledcWriteTone(BUZZER_CH, notes[i]);
    delay(150);
    buzzerOff();
    delay(30);
  }
}

void openSafe()
{
  servoWriteAngle(45);

  delay(3000);

  servoWriteAngle(0);
}

void setup()
{
  for (uint8_t i = 0; i < 7; i++)
  {
    pinMode(segPins[i], OUTPUT);
  }

  // Серво 50 Гц, 16 біт
  ledcSetup(SERVO_CH, 50, 16);
  ledcAttachPin(SERVO_PIN, SERVO_CH);

  // Бузер
  ledcSetup(BUZZER_CH, 1000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);

  servoWriteAngle(0);

  showDigit(0);
}

void loop()
{
  enc.tick();

  if (enc.right())
  {
    currentDigit++;

    if (currentDigit > 9)
      currentDigit = 0;

    showDigit(currentDigit);
  }

  if (enc.left())
  {
    currentDigit--;

    if (currentDigit < 0)
      currentDigit = 9;

    showDigit(currentDigit);
  }

  if (enc.click())
  {
    if (currentDigit == secretDigit)
    {
      playSuccessMelody();
      openSafe();
    }
    else
    {
      beepError();
    }
  }
}