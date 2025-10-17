#pragma once

#include <Arduino.h>

void initLogger();
void logThrottled(const char* format, ...);
void queueWebSerial(const char* message);
void processWebSerialQueue();