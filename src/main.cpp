/*
  ESP32 Growbox Watering System
  Version 0.9.1
  
  This code is distributed under GNU Lesser General Public License
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "utils/logger.h"
#include "hardware/pin_manager.h"
#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "network/wifi_manager.h"
#include "network/websocket_handler.h"
#include "network/ntp_manager.h"
#include "storage/filesystem_manager.h"
#include "storage/config_manager.h"
#include "scheduler/job_processor.h"
#include "scheduler/job_state_machine.h"

// Define version
const char* APP_VERSION = "0.9.1";

// Define NTP servers
const char* ntpServer1 = "fritz.box";
const char* ntpServer2 = "pool.ntp.org";
const char* ntpServer3 = "time.nist.gov";
const long gmtOffset_sec = 3600;  // GMT+1
const int daylightOffset_sec = 3600;  // DST offset
const char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Central European Time

// Define WiFi credentials
const char* ssid_ap = "GrowboxAP";
const char* password_ap = "xxxxxxxxxxx";
const char* ssid_sta = "XYZ-WLAN";  // Change to your WiFi SSID
const char* password_sta = "xxxxxxxxxxxx";  // Change to your WiFi password
const int WiFiMode_AP_STA = 1;  // 0 = AP mode, 1 = STA mode

// Global state variables
Settings settings;
std::vector<jobStruct> joblistVec;
std::vector<int> valvePins;
std::vector<bool> valve_switches;
std::vector<int> valveStates;

bool auto_switch = false;
bool pump_switch = false;

int pumpState = 0;
float pumpRunTime = 0;
unsigned long pumpStartMillis = 0;

bool jobActive = false;
JobState currentJobState = JOB_IDLE;
unsigned long jobStateTimestamp = 0;
jobStruct runningJob;

PumpContext pumpCtx = {PUMP_IDLE, 0, false, false};
NtpContext ntpCtx = {NTP_IDLE, 0, 0, false, 0};

// Flow sensor variables
volatile int pulseCount = 0;
float soilFlowRate = 0.0;
float soilFlowVolume = 0.0;
float roundSoilFlowVolume = 0.0;
float tempsoilFlowVolume = 0.0;

// Timing variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

static unsigned long lastJobCheck = 0;
volatile bool otaUpdating = false;

// Server and file paths
AsyncWebServer server(80);
const char* configfile = "/config.json";
const char* jobsfile = "/schedules.json";

void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

void calculateSoilFlowRate() {
    detachInterrupt(digitalPinToInterrupt(soilFlowSensorPin));
    soilFlowRate = ((1000.0 / (millis() - lastTime)) * pulseCount) / 7.5;
    soilFlowVolume += (soilFlowRate / 60.0);
    roundSoilFlowVolume = round(soilFlowVolume * 100) / 100;
    
    if (roundSoilFlowVolume != tempsoilFlowVolume) {
        tempsoilFlowVolume = roundSoilFlowVolume;
    }
    
    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);
}

void recvMsg(uint8_t *data, size_t len) {
    logThrottled("Received Data...");
    String d = "";
    for(int i = 0; i < len; i++) {
        d += char(data[i]);
    }
    logThrottled("%s", d.c_str());
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not Found\n\n");
}

void setup() {
    initializePins();
    initLogger();
    
    Serial.printf("Application version: %s\n", APP_VERSION);
    
    initFS();
    initWiFi();
    initWebSocket();
    
    pinMode(pumpPin, OUTPUT);
    pinMode(soilFlowSensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);
    
    digitalWrite(pumpPin, LOW);
    delay(50);
    
    pumpState = digitalRead(pumpPin);
    
    WebSerial.begin(&server);
    WebSerial.onMessage(recvMsg);
    
    ArduinoOTA.setHostname("GrowboxWatering");
    ArduinoOTA.onStart([]() {
        otaUpdating = true;
        logThrottled("OTA start");
    });
    ArduinoOTA.onEnd([]() {
        otaUpdating = false;
        logThrottled("OTA end");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned long lastProgressLog = 0;
        unsigned long now = millis();
        if (now - lastProgressLog > 1000) {
            lastProgressLog = now;
            logThrottled("OTA Progress: %u%%", (progress / (total / 100)));
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        otaUpdating = false;
        logThrottled("OTA Error[%u]", error);
    });
    ArduinoOTA.begin();
    
    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);
    server.serveStatic("/", LittleFS, "/");
    server.serveStatic("/js/", LittleFS, "/js/");
    server.serveStatic("/css/", LittleFS, "/css/");
    server.serveStatic("/lang/", LittleFS, "/lang/");
    
    server.addHandler(&ws);
    server.begin();
    
    logThrottled("HTTP server started");
    
    ntpCtx.state = NTP_INIT;
    ntpCtx.stateTime = millis();
    ntpCtx.syncInProgress = true;
    
    loadConfiguration(configfile);
    loadJobList(jobsfile);
}

void loop() {
    ArduinoOTA.handle();
    ws.cleanupClients();
    
    handleJobStateMachine();
    
    if (auto_switch) {
        unsigned long now = millis();
        if (now - lastJobCheck >= JOB_CHECK_INTERVAL) {
            jobsProcessor();
            lastJobCheck = now;
        }
    }
    
    if (pumpState == HIGH) {
        if ((millis() - lastTime) > timerDelay) {
            pumpRunTime = (millis() - pumpStartMillis) / 1000.0f;
            calculateSoilFlowRate();
            notifyClients();
            lastTime = millis();
        }
    }
    
    processWebSerialQueue();
    handleNTPSync();
    
    yield();
}