
#include <Arduino.h>

#define RED_LED     8
#define YELLOW_LED  18
#define GREEN_LED   17

#define SEG_A  4
#define SEG_B  5
#define SEG_C  6
#define SEG_D  7
#define SEG_E  9
#define SEG_F  10
#define SEG_G  11

#define RED_TIME_S         7
#define RED_YELLOW_TIME_S  2
#define GREEN_TIME_S       7
#define YELLOW_TIME_S      2

#define GREEN_BLINK_TIME_MS 500

TaskHandle_t trafficTaskHandle = NULL;

const bool digits[10][7] =
{
    {0,0,0,0,0,0,1}, // 0
    {1,0,0,1,1,1,1}, // 1
    {0,0,1,0,0,1,0}, // 2
    {0,0,0,0,1,1,0}, // 3
    {1,0,0,1,1,0,0}, // 4
    {0,1,0,0,1,0,0}, // 5
    {0,1,0,0,0,0,0}, // 6
    {0,0,0,1,1,1,1}, // 7
    {0,0,0,0,0,0,0}, // 8
    {0,0,0,0,1,0,0}  // 9
};

void displayDigit(uint8_t digit)
{
    if (digit > 9) digit = 9;

    digitalWrite(SEG_A, digits[digit][0]);
    digitalWrite(SEG_B, digits[digit][1]);
    digitalWrite(SEG_C, digits[digit][2]);
    digitalWrite(SEG_D, digits[digit][3]);
    digitalWrite(SEG_E, digits[digit][4]);
    digitalWrite(SEG_F, digits[digit][5]);
    digitalWrite(SEG_G, digits[digit][6]);
}

void clearDisplay()
{
    digitalWrite(SEG_A, HIGH);
    digitalWrite(SEG_B, HIGH);
    digitalWrite(SEG_C, HIGH);
    digitalWrite(SEG_D, HIGH);
    digitalWrite(SEG_E, HIGH);
    digitalWrite(SEG_F, HIGH);
    digitalWrite(SEG_G, HIGH);
}

void countdown(uint8_t seconds)
{
    for (int i = seconds; i > 0; i--)
    {
        displayDigit(i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void trafficLightTask(void *pvParameters)
{
    while (true)
    {
        digitalWrite(RED_LED, HIGH);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(GREEN_LED, LOW);

        countdown(RED_TIME_S);

    
        digitalWrite(RED_LED, HIGH);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);

        countdown(RED_YELLOW_TIME_S);

        digitalWrite(RED_LED, LOW);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);

        
        for (int sec = 7; sec > 3; sec--)
        {
            displayDigit(sec);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

    
        for (int sec = 3; sec > 0; sec--)
        {
            displayDigit(sec);

            digitalWrite(GREEN_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(GREEN_BLINK_TIME_MS));

            digitalWrite(GREEN_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(GREEN_BLINK_TIME_MS));
        }

        digitalWrite(RED_LED, LOW);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);

        countdown(YELLOW_TIME_S);

        clearDisplay();
    }
}

void setup()
{
    pinMode(SEG_A, OUTPUT);
    pinMode(SEG_B, OUTPUT);
    pinMode(SEG_C, OUTPUT);
    pinMode(SEG_D, OUTPUT);
    pinMode(SEG_E, OUTPUT);
    pinMode(SEG_F, OUTPUT);
    pinMode(SEG_G, OUTPUT);

    clearDisplay();

    pinMode(RED_LED, OUTPUT);
    pinMode(YELLOW_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    xTaskCreatePinnedToCore(
        trafficLightTask,
        "TrafficLight",
        4096,
        NULL,
        1,
        &trafficTaskHandle,
        1
    );
}

void loop()
{
}