#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

void initWebSocket();
void notifyClients();
void handleGetData();
void handleGetSettings();
void handleSaveSettings(const JsonDocument& json);
void handleGetJobList();
void handleAddJobToList(const JsonDocument& json);
void handleAutoSwitch();
void handleResetCounter();
void handleGetMoistureSensors();

extern AsyncWebSocket ws;