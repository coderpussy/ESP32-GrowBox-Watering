#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

void initWiFi();
bool initWiFiStation();
void initWiFiAP();
String getWiFiSSID();
String getWiFiPassword();
void saveWiFiCredentials(String ssid, String password);
void resetWiFiSettings();
bool checkWiFiConnection();

extern Preferences preferences;