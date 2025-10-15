/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// ESP32 Growbox Watering System
// Controls water pump and water valve units for 3 plants in a grow tent
// Uses WebSerial for debugging and configuration
// Uses LittleFS for configuration and job scheduling
// Uses AsyncWebServer for web interface
// Uses ArduinoJson for JSON handling
// Uses NTP for time synchronization
// Uses WebSocket for real-time communication

// History
// Version 0.9 pre3, 14.08.2025, coderpussy
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "config.h"

#include <vector> // For dynamic array
#include <stdio.h> // for sscanf
#include <stdarg.h> // for va_list, va_start, va_end
#include <limits.h>

// WebSerial message queue
struct WebSerialMessage {
    char message[192];
    unsigned long timestamp;
};

static const size_t WEBSERIAL_QUEUE_SIZE = 32;
std::vector<WebSerialMessage> webSerialQueue;
unsigned long lastWebSerialFlush = 0;
static const unsigned long WEBSERIAL_FLUSH_INTERVAL = 0; // 0ms due to delivery lost // 50ms between messages

static const unsigned long JOB_CHECK_INTERVAL = 1000;  // Check jobs every second
static unsigned long lastJobCheck = 0;

// NTP sync options
const unsigned long ntpSyncInterval = 30 * 60 * 1000; // Sync every 30 minutes (in ms)
static const unsigned long NTP_WAIT_LOG_INTERVAL = 2000;  // 2 seconds between "waiting" messages

// NTP state machine
enum NTPState { 
    NTP_IDLE,
    NTP_INIT,
    NTP_WAITING,
    NTP_DONE
};

struct NTPContext {
    NTPState state;
    unsigned long stateTime;
    unsigned long lastSync;
    bool syncInProgress;
    int retryCount;
} ntpCtx = {NTP_IDLE, 0, 0, false, 0};

// Pump state machine
enum PumpState { 
    PUMP_IDLE,
    PUMP_STARTING,
    PUMP_RUNNING,
    PUMP_STOPPING
};

struct PumpContext {
    PumpState state;
    unsigned long stateTime;
    bool manualControl;
    bool targetState;  // true = on, false = off
} pumpCtx = {PUMP_IDLE, 0, false, false};

// Automatic watering switch
bool auto_switch = false;

// 12V Magnetic valve 1-3
std::vector<int> valvePins;
std::vector<bool> valve_switches;
std::vector<int> valveStates;

 // 12V Water Pump
int pumpPin = 33;

bool pump_switch = false;
float pumpStartTime = 0;
unsigned long pumpStartMillis = 0;
float pumpRunTime = 0;
float pumpTimeNow = 0;
int pumpState = 0;

// AZ-Delivery Moisture Sensor 1-3
int moistureSensorPin_1 = 16;
int moistureSensorPin_2 = 17;
int moistureSensorPin_3 = 18;

float moistureSensorValue_1 = 0;
float moistureSensorValue_2 = 0;
float moistureSensorValue_3 = 0;

int moistureSensorValuePercent_1 = 0;
int moistureSensorValuePercent_2 = 0;
int moistureSensorValuePercent_3 = 0;

int dryAnalogValue = 1023;
int wetAnalogValue = 0;
int DryValuePercent = 0;
int WetValuepercent = 100;

// Soil Flow Sensor
const int soilFlowSensorPin = 19; // Water Flow Rate Sensor YF-S401
volatile int pulseCount = 0;
float soilFlowRate = 0.0;
float soilFlowVolume = 0.0;
float roundSoilFlowVolume = 0.0;
float tempsoilFlowVolume = 0.0;

// initial action string value
String action;

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 1000;

// Initialize settings with default values
Settings settings;

// Joblist structure
struct jobStruct {
    int id;
    bool active;
    char name[25];
    char job[25];
    char plant[10];
    int duration;
    char starttime[25];
    bool everyday;
};

// Dynamic vector array for jobs
std::vector<jobStruct> joblistVec;

// Job date/time structure
struct jobDateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    bool valid;     // if true, date/time was parsed correctly
    bool timeOnly;  // true if only time (hh:mm) was provided, false if full date/time (yyyy-mm-dd hh:mm)
};

// Job-State-Variables
enum JobState { JOB_IDLE, JOB_OPEN_VALVE, JOB_START_PUMP, JOB_RUNNING, JOB_STOP_PUMP, JOB_CLOSE_VALVE };
JobState currentJobState = JOB_IDLE;
unsigned long jobStateTimestamp = 0;
jobStruct runningJob;
bool jobActive = false;

// Logging variables
static const unsigned long LOG_THROTTLE_MS = 0; // 0ms due to delivery lost // 200ms minimal interval between log messages in ms
static unsigned long lastLogMillis = 0;
volatile bool otaUpdating = false; // true if OTA update is in progress

// Create asynchronous WebServer object on port 80
AsyncWebServer server(80);

// Create a asynchronous WebSocket object
AsyncWebSocket ws("/ws");

// Initialize function prototypes
void initializePins() {
    // Reset existing valve pins
    for(const auto& pin : valvePins) {
        digitalWrite(pin, LOW);
        pinMode(pin, INPUT);  // Reset to input to avoid floating
    }

    // Clear and resize vectors
    valvePins.clear();
    valve_switches.clear();
    valveStates.clear();

    // Resize arrays based on plant count
    valvePins.resize(settings.plant_count);
    valve_switches.resize(settings.plant_count, false);
    valveStates.resize(settings.plant_count, 0);

    // Initialize pins
    for(uint8_t i = 0; i < settings.plant_count; i++) {
        valvePins[i] = 25 + i;  // Starting from pin 25
        
        pinMode(valvePins[i], OUTPUT);
        digitalWrite(valvePins[i], LOW);
    }
}

// IRAM ATTR function to count pulses from the flow sensor
// This function is called in the ISR when a pulse is detected
// It increments the pulseCount variable, which is used to calculate flow rate and volume
// The IRAM_ATTR attribute ensures that this function is placed in the IRAM (Instruction RAM) of the ESP32,
void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

// Initialize Serial
void initSerial() {
    Serial.begin(115200); // 115200 is the speed used to print boot messages.
    Serial.setDebugOutput(false); // Show debug output
    Serial.println();
    Serial.println("Initializing serial connection DONE.");
}

// Initialize LittleFS
void initFS() {
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    } else {
        Serial.println("LittleFS mounted successfully");
    }
}

// Initialize WiFi
void initWiFi() {
    //bool LED = false;
    int wifi_retry = 0;

    // Init WLAN AP or STA
    if (WiFiMode_AP_STA == 0) {
        WiFi.mode(WIFI_AP);                              // WiFi Mode Access Point
        delay (100);
        WiFi.softAP(ssid_ap, password_ap);                     // AP name and password
        Serial.println("Start WLAN AP");
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Start WLAN Client DHCP");         // WiFi Mode Client with DHCP
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid_sta, password_sta);

        while (WiFi.status() != WL_CONNECTED) {           // Check connection
            wifi_retry++;
            Serial.print(".");
            delay(1000);
            //digitalWrite(D4, LED);
            //LED = ! LED;
            if (wifi_retry > 10) {
                Serial.println("\nReboot");                   // Reboot after 10 connection tries
                ESP.restart();
            }
        }
        //digitalWrite(D4, HIGH);

        // If connection successful show IP address in serial monitor
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid_sta);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());  //IP address assigned to your ESP
    }
}

// Queue a message for WebSerial output
void queueWebSerial(const char* message) {
    if (!settings.use_webserial) return;
    
    // Queue voll? Älteste Nachricht entfernen
    if (webSerialQueue.size() >= WEBSERIAL_QUEUE_SIZE) {
        webSerialQueue.erase(webSerialQueue.begin());
    }

    WebSerialMessage msg;
    strlcpy(msg.message, message, sizeof(msg.message));
    
    msg.timestamp = millis();
    webSerialQueue.push_back(msg);
}

// Process the WebSerial message queue
void processWebSerialQueue() {
    if (!settings.use_webserial || webSerialQueue.empty()) return;
    
    unsigned long now = millis();
    if (now - lastWebSerialFlush < WEBSERIAL_FLUSH_INTERVAL) return;

    // Execute first message in queue
    const WebSerialMessage& msg = webSerialQueue.front();
    WebSerial.println(msg.message);
    webSerialQueue.erase(webSerialQueue.begin());
    lastWebSerialFlush = now;
}

// Simplified throttled logging function to Serial and WebSerial
void logThrottled(const char* format, ...) {
    unsigned long now = millis();
    if (now - lastLogMillis < LOG_THROTTLE_MS) return;
    lastLogMillis = now;

    char buffer[192];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Print to Serial
    Serial.println(buffer);

    // Queue WebSerial message
    if (settings.use_webserial) {
        queueWebSerial(buffer);
    }
}

// Message callback of WebSerial
void recvMsg(uint8_t *data, size_t len){
    logThrottled("Received Data...");
    String d = "";
    for(int i=0; i < len; i++){
        d += char(data[i]);
    }
    logThrottled("%s", d.c_str());
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
    // Open file for reading
    File file = LittleFS.open(filename);

    if (!file) {
        logThrottled("Failed to read file");
        return;
    }

    // Read entire file content into String
    String content = file.readString();
    file.close();

    // Check if we got any content
    if (content.length() == 0) {
        logThrottled("File is empty");
        return;
    }

    // Log the entire content at once
    logThrottled("%s", content.c_str());
}

// Loads the configuration from a file
void loadConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        // Open file for reading
        File file = LittleFS.open(configfile, "r");

        // Calculate proper JSON document size
        const size_t capacity = JSON_OBJECT_SIZE(6) + 128; // Added buffer for strings
        DynamicJsonDocument doc(capacity);

        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            logThrottled("Failed to read configuration file: %s", error.c_str());
            return;
        }

        // Load settings from the JSON document
        settings.use_webserial = doc["use_webserial"] | false;
        settings.use_flowsensor = doc["use_flowsensor"] | false;
        settings.use_moisturesensor = doc["use_moisturesensor"] | false;
        settings.auto_switch_enabled = doc["auto_switch_enabled"] | false;
        settings.plant_count = doc["plant_count"] | 3;

        // Update global auto_switch variable
        if (settings.auto_switch_enabled) auto_switch = settings.auto_switch_enabled;

        // Initialize pins with new settings
        initializePins();

        logThrottled("Configuration loaded - Plants: %d, WebSerial: %d, FlowSensor: %d, MoistureSensor: %d, AutoSwitch: %d",
            settings.plant_count,
            settings.use_webserial,
            settings.use_flowsensor,
            settings.use_moisturesensor,
            settings.auto_switch_enabled);
    } else {
        logThrottled("No configuration file found, using defaults");
    }
}

// Saves the configuration to a file
void saveConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        // Delete existing file
        LittleFS.remove(configfile);
    }

    // Open new file for writing
    File file = LittleFS.open(configfile, "w");
    if (!file) {
        logThrottled("Failed to create configuration file");
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const uint8_t size = JSON_OBJECT_SIZE(6);
    StaticJsonDocument<size> doc;

    // Set settings values in JSON document
    doc["use_webserial"] = settings.use_webserial;
    doc["use_flowsensor"] = settings.use_flowsensor;
    doc["use_moisturesensor"] = settings.use_moisturesensor;
    doc["auto_switch_enabled"] = settings.auto_switch_enabled;
    doc["plant_count"] = settings.plant_count;

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        logThrottled("Failed to write to configuration file");
    }

    // Close the file
    file.close();

    // Debug: Print file data
    printFile(configfile);
}

// Loads the job schedules from a file
void loadJobList(const char* jobsfile) {
    if (!LittleFS.exists(jobsfile)) {
        logThrottled("Jobs file '%s' does not exist", jobsfile);
        return;
    }

    File file = LittleFS.open(jobsfile, "r");
    if (!file) {
        logThrottled("Failed to open jobs file for validation: %s", jobsfile);
        return;
    }

    // Read full content
    String content = file.readString();
    file.close();

    if (content.length() == 0) {
        logThrottled("Jobs file '%s' is empty", jobsfile);
        return;
    }

    // Provide a reasonable dynamic capacity based on file size (bounded)
    size_t cap = (size_t)content.length() * 2 + 1024;
    const size_t CAP_MAX = 64 * 1024; // cap to 64KB to avoid huge allocations
    if (cap > CAP_MAX) cap = CAP_MAX;

    DynamicJsonDocument doc(cap);
    DeserializationError err = deserializeJson(doc, content);

    if (err) {
        logThrottled("Invalid JSON in jobs file '%s': %s", jobsfile, err.c_str());
        return;
    }

    // If already an array -> OK
    if (doc.is<JsonArray>()) {
        logThrottled("Jobs file '%s' already top-level array", jobsfile);

        // File contains a JSON-Array at top level
        JsonArray arr = doc.as<JsonArray>();
        joblistVec.clear();

        for (JsonObject obj : arr) {
            jobStruct job;
            job.id = obj["id"] | 0;
            job.active = obj["active"] | false;
            strlcpy(job.name, obj["name"] | "", sizeof(job.name));
            strlcpy(job.job, obj["job"] | "", sizeof(job.job));
            strlcpy(job.plant, obj["plant"] | "", sizeof(job.plant));
            job.duration = obj["duration"] | 0;
            strlcpy(job.starttime, obj["starttime"] | "", sizeof(job.starttime));
            job.everyday = obj["everyday"] | false;
            joblistVec.push_back(job);
        }

        logThrottled("Loaded %d job(s)", joblistVec.size());
    }
}

// Saves the job schedules to a JSON file
void handleSaveJobList(const char* jobsfile) {
    if (LittleFS.exists(jobsfile)) {
        // Delete existing file
        if (!LittleFS.remove(jobsfile)) {
            logThrottled("Failed to remove existing jobs file");
            return;
        }
    }

    // Open file for writing
    File file = LittleFS.open(jobsfile, "w");
    if (!file) {
        logThrottled("Failed to create job schedules file");
        return;
    }

    // Get joblistVec array length
    int joblen = joblistVec.size();
    if (joblen == 0) {
        logThrottled("No jobs to save");
        file.close();
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const size_t capacity = JSON_ARRAY_SIZE(joblen) + 
                          (joblen * JSON_OBJECT_SIZE(8)) + // 8 properties per job
                          (joblen * 128); // Extra space for strings in each job
    
    // Use DynamicJsonDocument for runtime capacity
    DynamicJsonDocument doc(capacity);
    // convert joblistVec to JSON array
    JsonArray joblistArray = doc.to<JsonArray>();
    // Log the size of the document
    logThrottled("Preparing to save %d jobs with capacity %d bytes", joblen, capacity);

    // Iterate through the joblistVec and add each job to the JSON array
    for (size_t i = 0; i < joblen; i++) {
        const jobStruct& job = joblistVec[i];
        JsonObject obj = joblistArray.createNestedObject();
        
        // Debug output for each job
        logThrottled("Processing job %d: %s", i, job.name);

        // Add all job properties
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["job"] = job.job;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }

    // Serialize JSON to file
    size_t bytesWritten = serializeJson(doc, file);
    file.close();

    if (bytesWritten == 0) {
        logThrottled("Failed to write jobs to file");
        return;
    }

    logThrottled("Successfully saved %d jobs (%d bytes)", joblen, bytesWritten);
    printFile(jobsfile);
}

void handleGetData() {
    // Add safety checks
    if (valvePins.empty() || valve_switches.empty() || valveStates.empty()) {
        logThrottled("Error: Valve arrays not initialized");
        return;
    }

    String Text;
    
    const size_t capacity = JSON_OBJECT_SIZE(4) + 
                           JSON_ARRAY_SIZE(settings.plant_count) + 
                           settings.plant_count * JSON_OBJECT_SIZE(2) + 
                           128;
    DynamicJsonDocument root(capacity);

    root["action"] = "setvalues";
    root["auto_switch"] = auto_switch;
    root["pump_switch"] = pump_switch;
    root["pumpRunTime"] = String(pumpRunTime, 2);

    // Add valve states array
    JsonArray valveArray = root.createNestedArray("valves");
    for(uint8_t i = 0; i < settings.plant_count && i < valve_switches.size(); i++) {
        JsonObject valve = valveArray.createNestedObject();
        valve["id"] = i + 1;
        valve["state"] = bool(valve_switches[i]); // Explicitly convert to bool
    }

    if (settings.use_flowsensor) {
        root["soilFlowVolume"] = String(roundSoilFlowVolume, 3);
    }

    serializeJson(root, Text);
    ws.textAll(Text); // Send sensors values to websocket clients
}

void handleGetSettings() {
    String Text;

    const uint8_t size = JSON_OBJECT_SIZE(7);
    StaticJsonDocument<size> json;

    // Clear the JSON object
    json.clear();

    // Set action for the JSON object
    json["action"] = "setsettings";

    // Fill JSON object with settings values
    json["use_webserial"] = settings.use_webserial;
    json["use_flowsensor"] = settings.use_flowsensor;
    json["use_moisturesensor"] = settings.use_moisturesensor;
    json["auto_switch_enabled"] = settings.auto_switch_enabled;
    json["plant_count"] = settings.plant_count;

    serializeJson(json, Text);

    ws.textAll(Text); //Send values to websocket clients 
}

void handleSaveSettings(const JsonDocument& json) {
    String Text;
    
    // Save settings from JSON to settings structure
    settings.use_webserial = json["use_webserial"] | false;
    settings.use_flowsensor = json["use_flowsensor"] | false;
    settings.use_moisturesensor = json["use_moisturesensor"] | false;
    settings.auto_switch_enabled = json["auto_switch_enabled"] | false;
    
    // Update global auto_switch state
    if (settings.auto_switch_enabled) auto_switch = settings.auto_switch_enabled;
    
    // Handle plant count changes
    uint8_t new_count = json["plant_count"] | 3;
    if (new_count != settings.plant_count) {
        settings.plant_count = new_count;
        initializePins();  // Reinitialize with new count
    }

    saveConfiguration(configfile);
    // Send updated settings to websocket clients
    handleGetSettings();
}

void handleGetJobList() {
    // Calculate capacity for the entire document including action and joblist wrapper
    int arrayCount = joblistVec.empty() ? 1 : joblistVec.size();
    
    const size_t capacity = JSON_OBJECT_SIZE(2) +           // Main object with 2 properties (action, joblist)
                           JSON_ARRAY_SIZE(arrayCount) +    // Array size
                           (arrayCount * JSON_OBJECT_SIZE(8)) + // 8 properties per job
                           (arrayCount * 128);              // Extra space for strings
    
    DynamicJsonDocument doc(capacity);
    
    // Add action property
    doc["action"] = "setjoblist";
    
    // Create joblist array
    JsonArray joblistArray = doc.createNestedArray("joblist");
    
    // Debug output
    logThrottled("Preparing to load %d job(s) with capacity %d bytes", arrayCount, capacity);

    // Iterate through the joblistVec and add each job to the JSON array
    for (size_t i = 0; i < arrayCount && i < joblistVec.size(); i++) {
        const jobStruct& job = joblistVec[i];
        JsonObject obj = joblistArray.createNestedObject();
        
        // Debug output for each job
        logThrottled("Processing job %d: %s", i, job.name);

        // Add all job properties
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["job"] = job.job;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }

    String Text;
    serializeJson(doc, Text);
    ws.textAll(Text); // Send joblist to websocket clients
}

// Delete the jobs file
void handleDeleteJobList(const char* jobsfile) {
    if (!LittleFS.exists(jobsfile)) {
        logThrottled("Jobs file does not exist");
        return;
    }

    if (!LittleFS.remove(jobsfile)) {
        logThrottled("Failed to delete jobs file");
        return;
    }

    logThrottled("Deleted jobs file");
    joblistVec.clear();
    handleGetJobList();
}

void handleAddJobToList(const JsonDocument& json) {
    // Add new json job data to dynamic jobListVec array
    jobStruct newJob;

    // Check if job ID already exists
    int newId = json["id"] | -1;
    if (newId < 0) {
        logThrottled("Invalid job id, skipping");
        return;
    }

    // If job ID is first element (0), clear existing job list
    if (newId == 0) {
        logThrottled("First job id, clearing existing job list");
        joblistVec.clear();
    }

    for (const jobStruct& job : joblistVec) {
        if (job.id == newId) {
            logThrottled("Job ID already exists, not adding to job list");
            return; // Exit if job ID already exists
        }
    }

    // If job ID does not exist, proceed to add new job
    logThrottled("Adding new job to job list");

    // Add new job to joblistVec from JSON data
    newJob.id = newId;
    newJob.active = json["active"] | false;
    strlcpy(newJob.name, json["name"] | "", sizeof(newJob.name));
    strlcpy(newJob.job, json["job"] | "", sizeof(newJob.job));
    strlcpy(newJob.plant, json["plant"] | "", sizeof(newJob.plant));
    newJob.duration = json["duration"] | 0;
    strlcpy(newJob.starttime, json["starttime"] | "", sizeof(newJob.starttime));
    newJob.everyday = json["everyday"] | false;
    joblistVec.push_back(newJob);

    // Log new added job to logThrottled
    logThrottled("New Job - ID: %d, Active: %d, Name: %s, Job: %s, Plant: %s, Duration: %d, Starttime: %s, Everyday: %d",
        newJob.id,
        newJob.active,
        newJob.name,
        newJob.job,
        newJob.plant,
        newJob.duration,
        newJob.starttime,
        newJob.everyday
    );
}

void notifyClients() {
    handleGetData();
}

void handleAutoSwitch() {
    auto_switch = !auto_switch;

    //saveConfiguration(configfile);  // Save to file
    logThrottled("Auto %s", auto_switch ? "On" : "Off");

    notifyClients();
}

void handleValveSwitch(uint8_t valveNum) {
    if (valveNum >= settings.plant_count) {
        logThrottled("Invalid valve number: %d", valveNum);
        return;
    }
    
    bool currentState = valve_switches[valveNum];
    if (currentState) {
        // Only close if pump is not running
        if (pumpCtx.state != PUMP_RUNNING) {
            digitalWrite(valvePins[valveNum], LOW);
            valve_switches[valveNum] = false;
            valveStates[valveNum] = LOW;
            logThrottled("Valve %d closed", valveNum + 1);
        } else {
            logThrottled("Cannot close valve %d - pump is running", valveNum + 1);
        }
    } else {
        digitalWrite(valvePins[valveNum], HIGH);
        valve_switches[valveNum] = true;
        valveStates[valveNum] = HIGH;
        logThrottled("Valve %d opened", valveNum + 1);
    }

    notifyClients();
}

void handlePumpSwitch(bool manual = true) {
    unsigned long now = millis();
    bool stateChanged = false;
    bool anyValveOpen = false;

    // Handle manual toggle from frontend
    if (manual) {
        pumpCtx.manualControl = true;
        pumpCtx.targetState = !pump_switch;
        
        // Force immediate stop if turning off manually
        if (!pumpCtx.targetState) {
            pumpCtx.state = PUMP_STOPPING;
        } 
        // Only allow start if in IDLE state
        else if (pumpCtx.state == PUMP_IDLE) {
            pumpCtx.state = PUMP_STARTING;
        }
        pumpCtx.stateTime = now;
    }

    // State machine
    switch (pumpCtx.state) {
        case PUMP_IDLE:
            // Wait for commands
            break;

        case PUMP_STARTING:
            // Check if any valve is open
            anyValveOpen = false;  // Reset flag
            for(uint8_t i = 0; i < settings.plant_count; i++) {
                if (valveStates[i] == HIGH) {
                    anyValveOpen = true;
                    break;
                }
            }
            // Check if at least one valve is open before starting pump
            if (anyValveOpen) {
                digitalWrite(pumpPin, HIGH);
                pump_switch = true;
                pumpCtx.state = PUMP_RUNNING;

                pumpStartTime = now;
                pumpStartMillis = now;
                
                logThrottled("Pump starting %s", pumpCtx.manualControl ? "(manual)" : "(auto)");
                stateChanged = true;
            } else {
                logThrottled("Cannot start pump - no valve open");
                pumpCtx.state = PUMP_IDLE;

                pump_switch = false;  // Ensure switch shows correct state
                stateChanged = true;
            }
            break;

        case PUMP_RUNNING:
            if (!pumpCtx.targetState) {
                pumpCtx.state = PUMP_STOPPING;
                pumpCtx.stateTime = now;
            }

            // Update runtime while running
            pumpRunTime = (now - pumpStartMillis) / 1000.0f;
            break;

        case PUMP_STOPPING:
            digitalWrite(pumpPin, LOW);
            pump_switch = false;
            pumpCtx.state = PUMP_IDLE;

            // Final runtime calculation
            pumpRunTime = (now - pumpStartMillis) / 1000.0f;
            logThrottled("Pump stopping %s after %.1f seconds", 
                pumpCtx.manualControl ? "(manual)" : "(auto)",
                pumpRunTime);
            stateChanged = true;
            break;
    }

    // Update pump state and notify if changed
    if (stateChanged) {
        pumpState = digitalRead(pumpPin);
        if (!pump_switch) {
            if (!manual) {
                pumpRunTime = 0;  // Reset runtime when pump stops
                pumpStartMillis = 0;
            }
        }
        notifyClients();
    }
}

void handleResetCounter() {
    pulseCount = 0;
    soilFlowRate = 0.0;
    soilFlowVolume = 0.0;
    roundSoilFlowVolume = 0.0;
    tempsoilFlowVolume = 0.0;

    pumpStartTime = 0;
    pumpRunTime = 0;
    pumpStartMillis = 0;

    notifyClients();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        const uint8_t size = JSON_OBJECT_SIZE(9);
        StaticJsonDocument<size> json;
        DeserializationError err = deserializeJson(json, data);

        if (err) {
            logThrottled("deserializeJson() failed with code %s", err.c_str());
            return;
        }

        // copy action string; avoid pointer to JsonDocument internals
        const char* tmpAct = json["action"] | "";
        action = String(tmpAct);

        logThrottled("action: %s", action.c_str());

        if (action == "getvalues") handleGetData();
        else if (action == "getsettings") handleGetSettings();
        else if (action == "getjoblist") handleGetJobList();
        else if (action == "savesettings") handleSaveSettings(json);
        else if (action == "addjobtolist") handleAddJobToList(json);
        else if (action == "savejoblist") handleSaveJobList(jobsfile);
        else if (action == "deletejoblist") handleDeleteJobList(jobsfile);
        else if (action == "resetcounter") handleResetCounter();
        else if (action == "auto_switch") handleAutoSwitch();
        else if (action == "pump_switch") handlePumpSwitch(true);
        else if (action == "valve_switch") {
            // Parse valve ID and state
            int valveId = json["valve_id"].as<int>();
            if (valveId > 0 && valveId <= settings.plant_count) {
                handleValveSwitch(valveId - 1);  // Convert to 0-based index
            }
        } else {
            // Should not happen
            logThrottled("Unknown action: %s", action.c_str());
        }
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            logThrottled("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        break;
        case WS_EVT_DISCONNECT:
            logThrottled("WebSocket client #%u disconnected", client->id());
        break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
        break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            logThrottled("WebSocket error: client #%u, error: %s", client->id(), (char*)data);
        break;
    }
}

void initWebSocket() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html"); //Send index web page
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not Found\n\n"); // Unknown request. Send error 404
}

void calculateSoilFlowRate() {
    // Calculate Soil Flowrate
    detachInterrupt(digitalPinToInterrupt(soilFlowSensorPin));
    soilFlowRate = ((1000.0 / (millis() - lastTime)) * pulseCount) / 7.5; // L/min
    soilFlowVolume += (soilFlowRate / 60.0); // Convert to liters
    roundSoilFlowVolume = round(soilFlowVolume*100)/100;

    if (roundSoilFlowVolume != tempsoilFlowVolume) {
        tempsoilFlowVolume = roundSoilFlowVolume;

        /*Serial.print("Soil Flow Rate: ");
        Serial.println(roundSoilFlowVolume);
        WebSerial.print(F("Soil Flow Rate: "));
        WebSerial.println(roundSoilFlowVolume);*/
    }

    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);
}
  
/*void calculateMoistureSensorValues() {
    // Calculate Moisture Sensor 1 Values
    moistureSensorValue_1 = analogRead(moistureSensorPin_1);
    Serial.print("Analog Moisture 1 Value : ");
    Serial.println(moistureSensorValue_1);
    WebSerial.print(F("Analog Moisture 1 Value : "));
    WebSerial.println(moistureSensorValue_1);
    moistureSensorValuePercent_1 = map(moistureSensorValue_1, dryAnalogValue, wetAnalogValue, DryValuePercent, WetValuepercent);
    Serial.print("Moisture 1 percent: ");
    Serial.print(moistureSensorValuePercent_1);
    Serial.println("%");
    WebSerial.print(F("Moisture 1 percent: "));
    WebSerial.print(moistureSensorValuePercent_1);
    WebSerial.print(F("%"));

    if (moistureSensorValuePercent_1 == DryValuePercent) {
        Serial.println("Plant 1 is dry !!");
        WebSerial.print(F("Plant 1 is dry !!\n"));
        // Open magnetic valve
        digitalWrite(valvePin_1, HIGH);
        Serial.println("Valve Switch 1 open");
        WebSerial.print(F("Valve Switch 1 open\n"));
        // Turn on water pump
        digitalWrite(pumpPin, HIGH);
        Serial.println("Pump Switch open");
        WebSerial.print(F("Pump Switch open\n"));
    } else {
        // Turn off water pump
        digitalWrite(pumpPin, LOW);
        Serial.println("Pump Switch closed");
        WebSerial.print(F("Pump Switch closed\n"));
        // Close magnetic valve
        digitalWrite(valvePin_1, LOW);
        Serial.println("Valve Switch 1 closed");
        WebSerial.print(F("Valve Switch 1 closed\n"));
    }

    // Calculate Moisture Sensor 2 Values
    moistureSensorValue_2 = analogRead(moistureSensorPin_2);
    Serial.print("Analog Moisture 2 Value : ");
    Serial.println(moistureSensorValue_2);
    WebSerial.print(F("Analog Moisture 2 Value : "));
    WebSerial.println(moistureSensorValue_2);
    moistureSensorValuePercent_2 = map(moistureSensorValue_2, dryAnalogValue, wetAnalogValue, DryValuePercent, WetValuepercent);
    Serial.print("Moisture 2 percent: ");
    Serial.print(moistureSensorValuePercent_2);
    Serial.println("%");
    WebSerial.print(F("Moisture 2 percent: "));
    WebSerial.print(moistureSensorValuePercent_2);
    WebSerial.print(F("%"));

    if (moistureSensorValuePercent_2 == DryValuePercent) {
        Serial.println("Plant 2 is dry !!");
        WebSerial.print(F("Plant 2 is dry !!\n"));
        // Open magnetic valve
        digitalWrite(valvePin_2, HIGH);
        Serial.println("Valve Switch 2 open");
        WebSerial.print(F("Valve Switch 2 open\n"));
        // Turn on water pump
        digitalWrite(pumpPin, HIGH);
        Serial.println("Pump Switch open");
        WebSerial.print(F("Pump Switch open\n"));
    } else {
        // Turn off water pump
        digitalWrite(pumpPin, LOW);
        Serial.println("Pump Switch closed");
        WebSerial.print(F("Pump Switch closed\n"));
        // Close magnetic valve
        digitalWrite(valvePin_2, LOW);
        Serial.println("Valve Switch 2 closed");
        WebSerial.print(F("Valve Switch 2 closed\n"));
    }

    // Calculate Moisture Sensor 3 Values
    moistureSensorValue_3 = analogRead(moistureSensorPin_3);
    Serial.print("Analog Moisture 3 Value : ");
    Serial.println(moistureSensorValue_3);
    WebSerial.print(F("Analog Moisture 3 Value : "));
    WebSerial.println(moistureSensorValue_3);
    moistureSensorValuePercent_3 = map(moistureSensorValue_3, dryAnalogValue, wetAnalogValue, DryValuePercent, WetValuepercent);
    Serial.print("Moisture 3 percent: ");
    Serial.print(moistureSensorValuePercent_3);
    Serial.println("%");
    WebSerial.print(F("Moisture 3 percent: "));
    WebSerial.print(moistureSensorValuePercent_3);
    WebSerial.print(F("%"));

    if (moistureSensorValuePercent_3 == DryValuePercent) {
        Serial.println("Plant 3 is dry !!");
        Serial.print(F("Plant 2 is dry !!\n"));
        // Open magnetic valve
        digitalWrite(valvePin_3, HIGH);
        Serial.println("Valve Switch 3 open");
        WebSerial.print(F("Valve Switch 3 open\n"));
        // Turn on water pump
        digitalWrite(pumpPin, HIGH);
        Serial.println("Pump Switch open");
        WebSerial.print(F("Pump Switch open\n"));
    } else {
        // Turn off water pump
        digitalWrite(pumpPin, LOW);
        Serial.println("Pump Switch closed");
        WebSerial.print(F("Pump Switch closed\n"));
        // Close magnetic valve
        digitalWrite(valvePin_3, LOW);
        Serial.println("Valve Switch 2 closed");
        WebSerial.print(F("Valve Switch 2 closed\n"));
    }
}*/

void handleNTPSync() {
    if (otaUpdating || jobActive) return;
    
    unsigned long now = millis();
    
    switch(ntpCtx.state) {
        case NTP_IDLE:
            if (now - ntpCtx.lastSync >= ntpSyncInterval) {
                ntpCtx.state = NTP_INIT;
                ntpCtx.stateTime = now;
                ntpCtx.syncInProgress = true;
                ntpCtx.retryCount = 0;
                logThrottled("Starting NTP sync...");
            }
            break;
            
        case NTP_INIT:
            logThrottled("Configuring NTP...");
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
            ntpCtx.state = NTP_WAITING;
            ntpCtx.stateTime = now;
            break;
            
        case NTP_WAITING: {
            static unsigned long lastNTPWaitLog = 0;
            time_t timeNow = time(nullptr);

            // Only log waiting message every NTP_WAIT_LOG_INTERVAL
            if (now - lastNTPWaitLog >= NTP_WAIT_LOG_INTERVAL) {
                logThrottled("Waiting for valid time... Current: %ld", (long)timeNow);
                lastNTPWaitLog = now;
            }

            if (timeNow > 24 * 3600) {
                // Valid time received
                struct tm timeinfo;
                localtime_r(&timeNow, &timeinfo);
                setenv("TZ", timezone, 1);
                tzset();
                ntpCtx.state = NTP_DONE;
                ntpCtx.lastSync = now;
                ntpCtx.syncInProgress = false;
                logThrottled("NTP sync complete - Time set to: %04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                // Timeout after 10 seconds
                if (now - ntpCtx.stateTime > 10000) {
                    if (++ntpCtx.retryCount >= 3) {
                        // Give up after 3 retries
                        ntpCtx.state = NTP_IDLE;
                        ntpCtx.syncInProgress = false;
                        logThrottled("NTP sync failed after 3 retries");
                    } else {
                        // Retry
                        ntpCtx.state = NTP_INIT;
                        logThrottled("NTP sync retry %d/3", ntpCtx.retryCount);
                    }
                }
            }
            break;
        }
            
        case NTP_DONE:
            ntpCtx.state = NTP_IDLE;
            break;
    }
}

// Helper function to parse job start time from string
jobDateTime parseJobDateTime(const char* starttime) {
    jobDateTime dt = {0,0,0,0,0,false,false};
    if (starttime == nullptr || starttime[0] == '\0') return dt;

    // Try full ISO-like datetime "YYYY-MM-DDTHH:MM"
    int year=0, month=0, day=0, hour=0, minute=0;
    int matched;

    // Try full datetime with both separators (space or T)
    matched = sscanf(starttime, "%4d-%2d-%2d%*[T ]%2d:%2d", &year, &month, &day, &hour, &minute);
    
    if (matched == 5) {
        /*logThrottled("Parsed one-time job: %04d-%02d-%02d %02d:%02d", 
            year, month, day, hour, minute);*/
            
        if (year >= 1970 && month >= 1 && month <= 12 && 
            day >= 1 && day <= 31 && 
            hour >= 0 && hour <= 23 && 
            minute >= 0 && minute <= 59) {
            dt.year = year;
            dt.month = month;
            dt.day = day;
            dt.hour = hour;
            dt.minute = minute;
            dt.valid = true;
            dt.timeOnly = false;
            return dt;
        }
        logThrottled("Invalid date/time ranges in one-time job");
        return dt;
    }

    // Try time-only format (HH:MM)
    matched = sscanf(starttime, "%2d:%2d", &hour, &minute);
    if (matched == 2) {
        //logThrottled("Parsed everyday job: %02d:%02d", hour, minute);
            
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
            dt.hour = hour;
            dt.minute = minute;
            dt.valid = true;
            dt.timeOnly = true;
            return dt;
        }
        logThrottled("Invalid time ranges in everyday job");
        return dt;
    }

    logThrottled("Failed to parse datetime: %s", starttime);
    return dt;
}

// This function will be called when a job is due to run
// Start job (without delay!)
void processJob(const jobStruct& job) {
    // guard: only one job at a time and pump must be free
    if (jobActive) {
        logThrottled("Another job is active, skipping start");
        return;
    }
    // copy job and start state machine
    runningJob = job;
    currentJobState = JOB_OPEN_VALVE;
    jobStateTimestamp = millis();
    jobActive = true;
    logThrottled("Start background job: %s for plant: %s\n", job.job, job.plant);
}

// This function processes the jobs based on the joblistVec
// Fine control: startWindowSec defines a time window to start the job on time
// No OTA updates or other jobs should be active when starting a job
void jobsProcessor() {
    static int lastCheckedSecond = -1;
    static int lastExecutedJobId = -1;
    static unsigned long lastJobStartTime = 0;
    const int startWindowSec = 30;

    time_t now_t = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now_t, &timeinfo);

    // Check every second instead of every minute
    int currentSecond = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    if (currentSecond == lastCheckedSecond) return;
    lastCheckedSecond = currentSecond;

    // Reset at midnight
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        lastExecutedJobId = -1;
    }

    if (otaUpdating) {
        logThrottled("OTA in progress - skipping job evaluation");
        return;
    }

    unsigned long now = millis();
    // Prevent re-execution within 60 seconds
    if (now - lastJobStartTime < 60000) {
        return;
    }

    for (const jobStruct& job : joblistVec) {
        if (job.id == lastExecutedJobId) continue;
        if (!job.active || job.starttime[0] == '\0') continue;

        jobDateTime dt = parseJobDateTime(job.starttime);
        if (!dt.valid) continue;

        bool shouldStart = false;
        if (job.everyday || dt.timeOnly) {
            int jobSecondsOfDay = dt.hour * 3600 + dt.minute * 60;
            int diff = currentSecond - jobSecondsOfDay;
            if (diff < 0) diff = -diff;
            
            /*logThrottled("Job %d time diff: %d seconds (window: %d)", 
                job.id, diff, startWindowSec);*/
                
            if (diff <= startWindowSec) shouldStart = true;
        } else {
            // One-time job handling
            struct tm jobtm = {0};
            jobtm.tm_year = dt.year - 1900;
            jobtm.tm_mon = dt.month - 1;
            jobtm.tm_mday = dt.day;
            jobtm.tm_hour = dt.hour;
            jobtm.tm_min = dt.minute;
            jobtm.tm_sec = 0;
            time_t jobEpoch = mktime(&jobtm);
            
            struct tm currenttm;
            localtime_r(&now_t, &currenttm);
            
            if (jobEpoch != (time_t)-1) {
                // Compare year, month, day
                if (currenttm.tm_year == (dt.year - 1900) &&
                    currenttm.tm_mon == (dt.month - 1) &&
                    currenttm.tm_mday == dt.day) {
                    
                    // If date matches, check time within window
                    int jobSecondsOfDay = dt.hour * 3600 + dt.minute * 60;
                    int currentSecondsOfDay = currenttm.tm_hour * 3600 + 
                                            currenttm.tm_min * 60 + 
                                            currenttm.tm_sec;
                    int diff = currentSecondsOfDay - jobSecondsOfDay;
                    if (diff < 0) diff = -diff;
                    
                    logThrottled("One-time job %d date matched, time diff: %d seconds", 
                        job.id, diff);
                        
                    if (diff <= startWindowSec) {
                        shouldStart = true;
                    }
                }
            }
        }

        if (shouldStart) {
            logThrottled("Job %d scheduled for %02d:%02d:%02d should start now", 
                job.id, dt.hour, dt.minute, timeinfo.tm_sec);

            if (jobActive) {
                logThrottled("Job %d due but another job active - skipping", job.id);
                continue;
            }

            processJob(job);
            lastExecutedJobId = job.id;
            lastJobStartTime = now;
            break;
        }
    }
}

// This function handles the job state machine
// It manages the different states of a job (idle, open valve, start pump, running, stop pump)
// and transitions between these states based on the job's duration and timing
void handleJobStateMachine() {
    if (!jobActive) return;

    unsigned long now = millis();
    switch (currentJobState) {
        case JOB_IDLE:
            // nothing
            break;
        case JOB_OPEN_VALVE: {
            int plantNum = 0;
            sscanf(runningJob.plant, "plant-%d", &plantNum);
            plantNum--; // Convert to 0-based index
            
            if (plantNum >= 0 && plantNum < settings.plant_count) {
                handleValveSwitch(plantNum);
                currentJobState = JOB_START_PUMP;
                jobStateTimestamp = now;
            } else {
                logThrottled("Invalid plant number in job - aborting");
                jobActive = false;
                currentJobState = JOB_IDLE;
            }
            break;
        }
        case JOB_START_PUMP:
            if (now - jobStateTimestamp >= 500) { // 500ms after opening valve
                // ensure at least one valve is open before starting pump
                pumpCtx.manualControl = false;
                pumpCtx.targetState = true;
                pumpCtx.state = PUMP_STARTING;

                handlePumpSwitch(false);  // false = not manual control
                
                if (pumpCtx.state == PUMP_RUNNING) {
                    logThrottled("Pump started for job: %s", runningJob.name);
                    currentJobState = JOB_RUNNING;
                    jobStateTimestamp = now;
                } else {
                    logThrottled("Failed to start pump for job - aborting");
                    jobActive = false;
                    currentJobState = JOB_IDLE;
                }
            }
            break;
        case JOB_RUNNING:
            // running duration assumed in seconds
            if (now - jobStateTimestamp >= (unsigned long)runningJob.duration * 1000UL) {
                pumpCtx.manualControl = false;
                pumpCtx.targetState = false;
                pumpCtx.state = PUMP_STOPPING;  // Force state change

                handlePumpSwitch(false);

                currentJobState = JOB_STOP_PUMP;
                jobStateTimestamp = now;
                logThrottled("Job duration complete, stopping pump");
            }
            break;
        case JOB_STOP_PUMP:
            if (now - jobStateTimestamp >= 750) { // wait 750ms after pump off
                // Get plant number from job
                int plantNum = 0;
                sscanf(runningJob.plant, "plant-%d", &plantNum);
                plantNum--; // Convert to 0-based index
                
                if (plantNum >= 0 && plantNum < settings.plant_count) {
                    handleValveSwitch(plantNum);
                    currentJobState = JOB_CLOSE_VALVE;
                    jobStateTimestamp = now;
                } else {
                    logThrottled("Invalid plant number %d - aborting", plantNum + 1);
                    jobActive = false;
                    currentJobState = JOB_IDLE;
                }
            }
            break;
        case JOB_CLOSE_VALVE:
            // finalize and reset
            jobActive = false;
            currentJobState = JOB_IDLE;
            logThrottled("Job finished.");
            notifyClients();
            break;
    }
}

void setup(void) {
    // Initialize pins
    initializePins();

    // Start serial for debugging
    initSerial();
    Serial.printf("Application version: %s\n", APP_VERSION);

    // Initialize file system and WiFi
    initFS();
    initWiFi();

    // Initialize WebSocket
    initWebSocket();

    /*pinMode(valvePin_1, OUTPUT);
    pinMode(valvePin_2, OUTPUT);
    pinMode(valvePin_3, OUTPUT);*/
    pinMode(pumpPin, OUTPUT);

    pinMode(moistureSensorPin_1, INPUT);
    pinMode(moistureSensorPin_2, INPUT);
    pinMode(moistureSensorPin_3, INPUT);
    pinMode(soilFlowSensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);

    /*digitalWrite(valvePin_1, LOW);
    digitalWrite(valvePin_2, LOW);
    digitalWrite(valvePin_3, LOW);*/
    digitalWrite(pumpPin, LOW);
    delay(50);

    // read initial states
    /*valve1State = digitalRead(valvePin_1);
    valve2State = digitalRead(valvePin_2);
    valve3State = digitalRead(valvePin_3);*/
    pumpState = digitalRead(pumpPin);

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    // Attach Message Callback
    WebSerial.onMessage(recvMsg);

    // Arduino OTA config and start
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
        // Throttle progress logs
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

    // Set HTTP server directives
    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);

    server.serveStatic("/", LittleFS, "/");
    server.serveStatic("/js/", LittleFS, "/js/");
    server.serveStatic("/css/", LittleFS, "/css/");
    server.serveStatic("/lang/", LittleFS, "/lang/");

    // Start HTTP server
    server.begin();
    logThrottled("HTTP server started");

    // Initialize NTP context
    ntpCtx.state = NTP_INIT;
    ntpCtx.stateTime = millis();
    ntpCtx.syncInProgress = true;

    // Load JSON configuration
    loadConfiguration(configfile);

    // Load JSON job schedules
    loadJobList(jobsfile);
}

void loop(void) {
    // Set localtime
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Handle Arduino OTA
    ArduinoOTA.handle();

    // Websocket cleanup clients
    ws.cleanupClients();

    // Execute job state machine
    handleJobStateMachine();

    // Check if auto switch is enabled
    if (auto_switch) {
        unsigned long now = millis();
        if (now - lastJobCheck >= JOB_CHECK_INTERVAL) {
            // Execute jobs processing
            jobsProcessor();
            lastJobCheck = now;
        }
    }

    // If pump is running calculate runtime and flow rate
    if (pumpState == HIGH) {
        // Update timer
        if ((millis() - lastTime) > timerDelay) {
            // Stopwatch pump runtime
            pumpRunTime = (millis() - pumpStartMillis) / 1000.0f;
            // Calculate Soil Flowrate
            calculateSoilFlowRate();
            // Notify clients with updated data
            notifyClients();
            lastTime = millis();
        }
    }

    // Calculate Moisture Sensor Values
    //calculateMoistureSensorValues();

    // WebSerial Queue processing
    processWebSerialQueue();
    
    // Statt direkte NTP-Prüfung
    handleNTPSync();

    // Keep loop non-blocking but allow background tasks
    yield();
}