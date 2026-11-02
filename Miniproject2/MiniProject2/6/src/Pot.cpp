#include "Pot.hpp"
#include "Timer.hpp"
#include "HardwareConfig.hpp"
#include "Gpio.hpp"
// #include "esp_log.h"

static uint16_t movAvgValues[movAvgValuesCount] = {}; // 1 .. 4096, 0 - is not set
static uint8_t movAvgIndex = 0;

static uint8_t speedBooster = minSpeedValue;

bool checkAllMovAvgValuesExist()
{
    bool result = true;
    for (size_t i = 0; i < movAvgValuesCount; ++i)
    {
        result &= movAvgValues[i] > 0;
    }

    return result;
}

uint16_t getMovAvgValue()
{
    uint32_t sum = 0;
    for (size_t i = 0; i < movAvgValuesCount; ++i)
    {
        sum += movAvgValues[i];
    }

    return sum / movAvgValuesCount;
}

uint8_t calculateSpeedBooster(uint16_t movAvgValue)
{

    const uint8_t speedGradationPeriodsCount = maxSpeedValue - minSpeedValue;
    uint8_t newSpeedBooster = minSpeedValue + (static_cast<uint32_t>(movAvgValue) * speedGradationPeriodsCount) / maxSpeedRawValue;
    // ESP_LOGI("Pot.cpp", "newSpeedBooster: %d", newSpeedBooster);
    const uint16_t potThreasholdValue = (static_cast<uint32_t>(maxSpeedRawValue) * potThresholdToUpdatePercent) / (speedGradationPeriodsCount * 100);

    if (
        (newSpeedBooster == speedBooster) ||
        ((newSpeedBooster - speedBooster == 1) &&
             ((movAvgValue -
               ((newSpeedBooster - minSpeedValue) * static_cast<uint32_t>(maxSpeedRawValue) / speedGradationPeriodsCount + 1)) < potThreasholdValue)) ||
         ((speedBooster - newSpeedBooster == 1) &&
          (((newSpeedBooster +1 - minSpeedValue) * static_cast<uint32_t>(maxSpeedRawValue) / speedGradationPeriodsCount + 1) -
           movAvgValue) < potThreasholdValue)
        )
        return speedBooster;

    return newSpeedBooster;
}

void updateSpeed()
{
    const uint16_t rawValue = readPotValue();
    movAvgValues[movAvgIndex] = rawValue + 1;
    movAvgIndex = (movAvgIndex + 1) % movAvgValuesCount;

    if (!checkAllMovAvgValuesExist())
        return;

    const uint16_t movAvg = getMovAvgValue();

    const uint8_t newSpeedBooster = calculateSpeedBooster(movAvg);

    if (newSpeedBooster != speedBooster) {
        speedBooster = newSpeedBooster;
        updateTrafficLightTimerSpeed(speedBooster);
    }
}