#pragma once

#include <Arduino.h>
#include <vector>

struct MoistureSensorData {
    uint8_t pin;
    int analogValue;
    int percentValue;
    bool isDry;
};

void initMoistureSensors();
void readMoistureSensors();
std::vector<MoistureSensorData> getMoistureSensorData();
int mapMoistureToPercent(int analogValue);

extern std::vector<MoistureSensorData> moistureSensors;
extern const int MOISTURE_SENSOR_START_PIN;
extern const int DRY_ANALOG_VALUE;
extern const int WET_ANALOG_VALUE;
extern const int DRY_PERCENT;
extern const int WET_PERCENT;