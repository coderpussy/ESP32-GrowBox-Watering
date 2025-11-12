#pragma once

#include <Arduino.h>

void IRAM_ATTR pulseCounter();
void initFlowSensorPin();
void calculateSoilFlowRate();
void resetFlowSensor();

extern volatile int pulseCount;
extern float soilFlowRate;          // Flow rate in ml/min
extern float soilFlowVolume;       // Total volume in ml