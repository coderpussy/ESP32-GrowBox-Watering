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
static unsigned long lastWiFiCheck = 0;
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
    // If in AP mode (not connected to WiFi), redirect to WiFi manager
    if (WiFi.getMode() == WIFI_AP) {
        request->send(LittleFS, "/wifimanager.html", "text/html");
    } else {
        request->send(LittleFS, "/index.html", "text/html");
    }
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not Found\n\n");
}

/*void handleWiFiManager(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifimanager.html", "text/html");
}*/

void handleScan(AsyncWebServerRequest *request) {
    // Check if a scan is already running
    int scanStatus = WiFi.scanComplete();
    
    if (scanStatus == WIFI_SCAN_RUNNING) {
        request->send(409, "application/json", "{\"error\":\"Scan already in progress\"}");
        return;
    }
    
    // Delete old scan results if they exist
    if (scanStatus >= 0) {
        WiFi.scanDelete();
    }
    
    // Start async scan
    WiFi.scanNetworks(true);  // true = async mode
    
    // Send immediate response
    request->send(202, "application/json", "{\"status\":\"Scan started, please wait...\"}");
}

// Add a new handler to get scan results
void handleScanResults(AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_RUNNING) {
        request->send(202, "application/json", "{\"status\":\"scanning\",\"networks\":[]}");
        return;
    }
    
    if (n == WIFI_SCAN_FAILED) {
        request->send(500, "application/json", "{\"error\":\"WiFi scan failed\"}");
        WiFi.scanDelete();
        return;
    }
    
    if (n < 0) {
        request->send(500, "application/json", "{\"error\":\"Unknown scan error\"}");
        return;
    }
    
    // Build JSON response
    String json = "{\"status\":\"complete\",\"networks\":[";
    
    for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":\"" + String(WiFi.encryptionType(i)) + "\"";
        json += "}";
    }
    
    json += "]}";
    request->send(200, "application/json", json);
    WiFi.scanDelete();
}

void handleConnect(AsyncWebServerRequest *request) {
    String ssid = "";
    String password = "";
    
    if (request->hasParam("ssid", true)) {
        ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
        password = request->getParam("password", true)->value();
    }
    
    if (ssid == "") {
        request->send(400, "text/plain", "SSID is required");
        return;
    }
    
    // Save credentials
    saveWiFiCredentials(ssid, password);
    
    request->send(200, "text/plain", "Credentials saved. Connecting...");
    
    // Restart ESP32 to connect with new credentials
    delay(1000);
    ESP.restart();
}

void handleResetWiFi(AsyncWebServerRequest *request) {
    resetWiFiSettings();
    request->send(200, "text/plain", "WiFi settings reset. Restarting...");
    delay(1000);
    ESP.restart();
}

void setup() {
    initializePins();
    initLogger();
    
    Serial.printf("Application version: %s\n", APP_VERSION);
    
    initFS();
    initWiFi(); // Custom WiFi Manager
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
    
    // Add WiFi Manager routes
    //server.on("/wifimanager", HTTP_GET, handleWiFiManager);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/scan-results", HTTP_GET, handleScanResults);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/reset-wifi", HTTP_GET, handleResetWiFi);
    
    // Regular routes
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
    unsigned long now = millis();
    
    // Periodic WiFi connection check (every 30 seconds)
    if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
        // Only check if we're supposed to be in station mode
        // Check if scan is NOT running before attempting reconnection
        int scanStatus = WiFi.scanComplete();
        if (scanStatus != WIFI_SCAN_RUNNING && (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA)) {
            if (!checkWiFiConnection()) {
                logThrottled("WiFi reconnection failed, will retry in 30s");
            }
        }
        lastWiFiCheck = now;
    }

    // Only run main functionality if connected to WiFi in station mode
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        ws.cleanupClients();
        
        handleJobStateMachine();
        
        if (auto_switch) {
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
    } else {
        // In AP mode or disconnected, still handle web server and WebSerial
        ws.cleanupClients();
        processWebSerialQueue();
    }
    
    yield();
}