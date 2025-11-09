#pragma once

#include <Arduino.h>
#include <vector>

struct MoistureSensorData {
    uint8_t pin;
    int analogValue;
    int percentValue;
    bool isDry;
};

void readMoistureSensors();
std::vector<MoistureSensorData> getMoistureSensorData();
int mapMoistureToPercent(int analogValue, int wetValue, int dryValue);

extern std::vector<MoistureSensorData> moistureSensors;
extern const int DRY_ANALOG_VALUE_0;
extern const int WET_ANALOG_VALUE_0;
extern const int DRY_ANALOG_VALUE_1;
extern const int WET_ANALOG_VALUE_1;
extern const int DRY_ANALOG_VALUE_2;
extern const int WET_ANALOG_VALUE_2;
extern const int DRY_PERCENT;
extern const int WET_PERCENT;