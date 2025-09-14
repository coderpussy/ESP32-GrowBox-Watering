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


// NTP sync options
unsigned long lastNTPUpdate = 0; // Timestamp for the last NTP sync
const unsigned long ntpSyncInterval = 30 * 60 * 1000; // Sync every 30 minutes (in ms)

// Automatic watering switch
bool auto_switch = false;

// 12V Magnetic valve 1-3
int valvePin_1 = 25;
int valvePin_2 = 26;
int valvePin_3 = 27;

bool valve_switch_1 = false;
bool valve_switch_2 = false;
bool valve_switch_3 = false;
int valve1State = 0;
int valve2State = 0;
int valve3State = 0;

 // 12V Water Pump
int pumpPin = 33;

bool pump_switch = false;
float pumpStartTime = 0;
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

// variables to save values from HTML form
const char* act;

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
};

// Job-State-Variables
enum JobState { JOB_IDLE, JOB_OPEN_VALVE, JOB_START_PUMP, JOB_RUNNING, JOB_STOP_PUMP, JOB_CLOSE_VALVE };
JobState currentJobState = JOB_IDLE;
unsigned long jobStateTimestamp = 0;
jobStruct runningJob;
bool jobActive = false;

// Create asynchronous WebServer object on port 80
AsyncWebServer server(80);

// Create a asynchronous WebSocket object
AsyncWebSocket ws("/ws");

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

// Message callback of WebSerial
void recvMsg(uint8_t *data, size_t len){
    WebSerial.println("Received Data...");
    String d = "";
    for(int i=0; i < len; i++){
        d += char(data[i]);
    }
    WebSerial.println(d);
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
    // Open file for reading
    File file = LittleFS.open(filename);

    if (!file) {
        Serial.println(F("Failed to read file"));
        WebSerial.println(F("Failed to read file"));
    return;
    }

    // Extract each characters by one by one
    while (file.available()) {
        Serial.print(F((char)file.read()));
        WebSerial.print(F((char)file.read()));
    }
    
    Serial.println();
    WebSerial.println();

    // Close the file
    file.close();
}

// Loads the configuration from a file
void loadConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        // Open file for reading
        File file = LittleFS.open(configfile, "r");

        // Allocate a temporary JsonDocument
        // Don't forget to change the capacity to match your requirements.
        // Use arduinojson.org/v6/assistant to compute the capacity.
        const uint8_t size = JSON_OBJECT_SIZE(4);
        StaticJsonDocument<size> doc;

        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, file);

        // Close the file
        file.close();

        if (error) {
            Serial.println(F("Failed to read configuration file, using default configuration"));
            WebSerial.print(F("Failed to read configuration file, using default configuration\n"));
        } else {
            Serial.println(F("Successfully read configuration file, using saved configuration"));
            WebSerial.print(F("Successfully read configuration file, using saved configuration\n"));

            // Load settings from the JSON document
            settings.use_webserial = doc["use_webserial"] | false;
            settings.use_flowsensor = doc["use_flowsensor"] | false;
            settings.use_moisturesensor = doc["use_moisturesensor"] | false;
        }
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
        Serial.println(F("Failed to create configuration file"));
        WebSerial.print(F("Failed to create configuration file\n"));
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const uint8_t size = JSON_OBJECT_SIZE(4);
    StaticJsonDocument<size> doc;

    // Set settings values in JSON document
    doc["use_webserial"] = settings.use_webserial;
    doc["use_flowsensor"] = settings.use_flowsensor;
    doc["use_moisturesensor"] = settings.use_moisturesensor;

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to configuration file"));
        WebSerial.print(F("Failed to write to configuration file\n"));
    }

    // Close the file
    file.close();

    // Debug: Print file data
    printFile(configfile);
}

int countJsonObjectsInFile(const char* filename) {
    File file = LittleFS.open(filename, "r");
    if (!file) return 0;
    int count = 0;
    while (file.available()) {
        char c = file.read();
        if (c == '{') count++;
    }
    file.close();
    return count;
}

// Loads the job schedules from a file
void loadJobList(const char* jobsfile) {
    int jobCount = countJsonObjectsInFile(jobsfile);

    if (jobCount == 0) {
        Serial.println(F("No jobs found or file error"));
        WebSerial.print(F("No jobs found or file error\n"));
        return;
    }

    // Get xapacity based on job count
    const size_t capacity = JSON_ARRAY_SIZE(jobCount) + jobCount * JSON_OBJECT_SIZE(8);
    //StaticJsonDocument<2048> doc;
    DynamicJsonDocument doc(capacity); // Use DynamicJsonDocument for runtime capacity

    File file = LittleFS.open(jobsfile, "r");
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println(F("Failed to read job schedules file, using default job schedules"));
        Serial.println(error.c_str());
        WebSerial.print(F("Failed to read job schedules file, using default job schedules\n"));
        WebSerial.println(error.c_str());
        return;
    } else {
        Serial.println(F("Successfully read job schedules file, using saved job schedules"));
        WebSerial.print(F("Successfully read job schedules file, using saved job schedules\n"));

        // Assume: File contains a JSON-Array at top level
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

        Serial.printf("Loaded %d jobs\n", joblistVec.size());
        WebSerial.printf("Loaded %d jobs\n", joblistVec.size());
    }
}

// Saves the job schedules to a JSON file
void handleSaveJobList(const char* jobsfile) {
    if (LittleFS.exists(jobsfile)) {
        // Delete existing file
        LittleFS.remove(jobsfile);
    }

    // Open file for writing
    File file = LittleFS.open(jobsfile, "w");
    if (!file) {
        Serial.println(F("Failed to create job schedules file"));
        WebSerial.print(F("Failed to create job schedules file\n"));
        return;
    }

    // Get joblistVec array length
    int joblen = joblistVec.size();

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const size_t size = JSON_ARRAY_SIZE(joblen) + joblen*JSON_OBJECT_SIZE(8);
    DynamicJsonDocument doc(size); // Use DynamicJsonDocument for runtime capacity

    // convert joblistVec to JSON array
    JsonArray joblistArray = doc.to<JsonArray>();
    
    // Iterate through the joblistVec and add each job to the dynamic JSON array
    for (const jobStruct& job : joblistVec) {
        JsonObject obj = joblistArray.add();
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["job"] = job.job;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }

    // Clear jobListVec to prevent duplicate entries
    //joblistVec.clear();

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to job schedules file"));
        WebSerial.print(F("Failed to write to job schedules file\n"));
    }

    // Close the file
    file.close();

    // Debug: Print file data
    printFile(jobsfile);
}

void handleGetData() {
    String Text;

    const uint8_t size = JSON_OBJECT_SIZE(9);
    StaticJsonDocument<size> root;

    // Set action for the JSON object
    root["action"] = "setvalues";

    // Fill JSON object with values
    root["auto_switch"] = auto_switch;
    root["valve_switch_1"] = valve_switch_1;
    root["valve_switch_2"] = valve_switch_2;
    root["valve_switch_3"] = valve_switch_3;

    root["pump_switch"] = pump_switch;
    root["pumpRunTime"] = String(pumpRunTime, 2);

    root["soilFlowVolume"] = String(roundSoilFlowVolume, 3);

    serializeJson(root, Text);

    ws.textAll(Text); // Send sensors values to websocket clients
}

void handleGetSettings() {
    String Text;

    const uint8_t size = JSON_OBJECT_SIZE(5);
    StaticJsonDocument<size> json;

    // Clear the JSON object
    json.clear();

    // Set action for the JSON object
    json["action"] = "setsettings";

    // Fill JSON object with settings values
    json["use_webserial"] = settings.use_webserial;
    json["use_flowsensor"] = settings.use_flowsensor;
    json["use_moisturesensor"] = settings.use_moisturesensor;

    serializeJson(json, Text);

    ws.textAll(Text); //Send values to websocket clients 
}

void handleSaveSettings(const JsonDocument& json) {
    String Text;
    
    // Save settings from JSON to settings structure
    settings.use_webserial = json["use_webserial"] | false;
    settings.use_flowsensor = json["use_flowsensor"] | false;
    settings.use_moisturesensor = json["use_moisturesensor"] | false;

    saveConfiguration(configfile);

    //serializeJson(json, Text);

    //ws.textAll(Text);
}

void handleGetJobList() {
    // Create a JSON document from joblist array and send it to the client
    String Text;
    int arrayCount = 1;

    // Check if joblistVec is not empty
    // If it is empty, we will still create a JSON array with one empty object
    // This ensures that the JSON structure is always valid
    if (!joblistVec.empty()) {
        arrayCount = joblistVec.size();
    }

    // Create a JSON document with a size based on the number of jobs
    const size_t size = JSON_ARRAY_SIZE(arrayCount) + 
                        (arrayCount * JSON_OBJECT_SIZE(8)) + 
                        (1 * JSON_OBJECT_SIZE(3));
    DynamicJsonDocument doc(size); // Use DynamicJsonDocument for runtime capacity
    
    // Create a JSON array in the document
    JsonArray joblistArray = doc.to<JsonArray>();
    
    // Iterate through the joblistVec and add each job to the dynamic JSON array
    for (const jobStruct& job : joblistVec) {
        JsonObject obj = joblistArray.add();
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["job"] = job.job;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }
    // Set action for the JSON object
    doc["action"] = "setjoblist";
    
    // Set joblist array in the JSON document
    doc["joblist"] = joblistArray;

    // Serialize the JSON document to a string
    serializeJson(doc, Text);

    // Send the JSON string to all WebSocket clients
    ws.textAll(Text);
}

void handleAddJobToList(const JsonDocument& json) {
    // Add new json job data to dynamic jobListVec array
    jobStruct newJob;

    // Check if job ID already exists
    for (const jobStruct& job : joblistVec) {
        if (job.id == json["id"]) {
            Serial.println(F("Job ID already exists, not adding to job list"));
            WebSerial.print(F("Job ID already exists, not adding to job list\n"));
            return; // Exit if job ID already exists
        }
    }

    // If job ID does not exist, proceed to add new job
    Serial.println(F("Adding new job to job list"));
    WebSerial.print(F("Adding new job to job list\n"));

    // Add new job to joblistVec from JSON data
    newJob.id = json["id"] | 0;
    newJob.active = json["active"] | false;
    strlcpy(newJob.name, json["name"] | "", sizeof(newJob.name));
    strlcpy(newJob.job, json["job"] | "", sizeof(newJob.job));
    strlcpy(newJob.plant, json["plant"] | "", sizeof(newJob.plant));
    newJob.duration = json["duration"] | 0;
    strlcpy(newJob.starttime, json["starttime"] | "", sizeof(newJob.starttime));
    newJob.everyday = json["everyday"] | false;
    joblistVec.push_back(newJob);
}

void notifyClients() {
    handleGetData();
}

void handleAutoSwitch() {
    if (auto_switch) {
        auto_switch = false;
        Serial.println("Auto Off");
        WebSerial.print(F("Auto Off\n"));
    } else {
        auto_switch = true;
        Serial.println("Auto On");
        WebSerial.print(F("Auto Off\n"));
    }

    notifyClients();
}

void handleValveSwitch(int valveNum) {
    int* valvePin;
    bool* valve_switch;
    int* valveState;

    // Set pointers based on valve number
    switch (valveNum) {
        case 1:
            valvePin = &valvePin_1;
            valve_switch = &valve_switch_1;
            valveState = &valve1State;
            break;
        case 2:
            valvePin = &valvePin_2;
            valve_switch = &valve_switch_2;
            valveState = &valve2State;
            break;
        case 3:
            valvePin = &valvePin_3;
            valve_switch = &valve_switch_3;
            valveState = &valve3State;
            break;
        default:
            Serial.println("Invalid valve number!");
            WebSerial.print(F("Invalid valve number!\n"));
            return;
    }

    if (*valve_switch) {
        // Close valve only if pump isn't running
        if (pumpState == LOW) {
            digitalWrite(*valvePin, LOW);
            *valve_switch = false;
            Serial.printf("Valve Switch %d closed\n", valveNum);
            WebSerial.printf("Valve Switch %d closed\n", valveNum);
        }
    } else {
        digitalWrite(*valvePin, HIGH);
        *valve_switch = true;
        Serial.printf("Valve Switch %d open\n", valveNum);
        WebSerial.printf("Valve Switch %d open\n", valveNum);
    }

    *valveState = digitalRead(*valvePin);

    notifyClients();
}

void handlePumpSwitch() {
    if (pump_switch) {
        // Turn off water pump
        digitalWrite(pumpPin, LOW);
        pump_switch = false;
        Serial.println("Pump Switch closed");
        WebSerial.print(F("Pump Switch closed\n"));
    } else {
        // Start pump only if one of valve 1-3 is open
        if (valve1State == HIGH || valve2State == HIGH || valve3State == HIGH) {
            // Turn on water pump
            digitalWrite(pumpPin, HIGH);
            pump_switch = true;
            
            if (pumpRunTime == 0) {
                pumpStartTime = millis();
            } else {
                pumpTimeNow = millis();
                pumpStartTime = pumpTimeNow - pumpRunTime;
            }

            Serial.println("Pump Switch open");
            WebSerial.print(F("Pump Switch open\n"));
        }
    }

    // Read the state of the pump pin value
    pumpState = digitalRead(pumpPin);

    notifyClients();
}

void handleResetCounter() {
    pulseCount = 0;
    soilFlowRate = 0.0;
    soilFlowVolume = 0.0;
    roundSoilFlowVolume = 0.0;
    tempsoilFlowVolume = 0.0;

    pumpStartTime = 0;
    pumpRunTime = 0;

    notifyClients();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        const uint8_t size = JSON_OBJECT_SIZE(9);
        StaticJsonDocument<size> json;
        DeserializationError err = deserializeJson(json, data);

        if (err) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
            WebSerial.print(F("deserializeJson() failed with code "));
            WebSerial.println(err.c_str());
            return;
        }

        act = json["action"];
        action = act;

        Serial.print("action: ");
        Serial.println(action);
        WebSerial.print(F("action: "));
        WebSerial.println(action);

        if (action == "getvalues") handleGetData();
        else if (action == "getsettings") handleGetSettings();
        else if (action == "savesettings") handleSaveSettings(json);
        else if (action == "getjoblist") handleGetJobList();
        else if (action == "addjobtolist") handleAddJobToList(json);
        else if (action == "savejoblist") handleSaveJobList(jobsfile);
        else if (action == "resetcounter") handleResetCounter();
        else if (action == "auto_switch") handleAutoSwitch();
        else if (action == "valve_switch_1") handleValveSwitch(1); // Switch for valve 1
        else if (action == "valve_switch_2") handleValveSwitch(2); // Switch for valve 2
        else if (action == "valve_switch_3") handleValveSwitch(3); // Switch for valve 3
        else if (action == "pump_switch") handlePumpSwitch();
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
        break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
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
    detachInterrupt(soilFlowSensorPin);
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

void syncTime() {
    Serial.print("Synchronizing time with NTP server...");
    WebSerial.print(F("Synchronizing time with NTP server..."));

    configTime(0, 0, "fritz.box", "pool.ntp.org", "time.nist.gov"); // UTC offset set to 0
    time_t now = time(nullptr);
    
    while (now < 24 * 3600) { // Wait until time is valid
        delay(100);
        now = time(nullptr);
    }
    Serial.println(" Time synchronized!");
    WebSerial.print(F(" Time synchronized!\n"));

    // Set timezone
    setenv("TZ", timezone, 1);
    tzset();

    lastNTPUpdate = millis(); // Record the time of the last sync
}

// Helper function to parse job start time from string
jobDateTime parseJobDateTime(const char* starttime) {
    jobDateTime dt;
    // Expect format: "YYYY-MM-DDTHH:MM"
    dt.year   = atoi(String(starttime).substring(0, 4).c_str());
    dt.month  = atoi(String(starttime).substring(5, 7).c_str());
    dt.day    = atoi(String(starttime).substring(8, 10).c_str());
    dt.hour   = atoi(String(starttime).substring(11, 13).c_str());
    dt.minute = atoi(String(starttime).substring(14, 16).c_str());
    return dt;
}

// This function will be called when a job is due to run
// Start job (without delay!)
void processJob(const jobStruct& job) {
    if (pumpState == LOW && !jobActive) {
        runningJob = job;
        currentJobState = JOB_OPEN_VALVE;
        jobStateTimestamp = millis();
        jobActive = true;
        Serial.printf("Start background job: %s for plant: %s\n", job.job, job.plant);
        WebSerial.printf("Start background job: %s for plant: %s\n", job.job, job.plant);
    }
}

// This function processes the jobs based on the joblistVec
// It checks the current time against the job start times and executes jobs that are due
// It supports both daily jobs and one-time jobs based on the job's start time
void jobsProcessor() {
    // Process jobs based on the joblistVec
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Iterate through the joblistVec to check for jobs to run
    for (const jobStruct& job : joblistVec) {
        if (job.active) {
            jobDateTime dt = parseJobDateTime(job.starttime);

            // Check if the job is set to run every day or on a specific date and time
            // Compare time based on the job's everyday flag
            if (job.everyday) {
                if (timeinfo.tm_hour == dt.hour && timeinfo.tm_min == dt.minute) {
                    // Execute job
                    processJob(job);
                }
            } else {
                // Compare date/time for one-time jobs
                // Check if the current date and time matches the job's scheduled date and time
                if ((timeinfo.tm_year + 1900 == dt.year) &&
                    (timeinfo.tm_mon + 1 == dt.month) &&
                    (timeinfo.tm_mday == dt.day) &&
                    (timeinfo.tm_hour == dt.hour) &&
                    (timeinfo.tm_min == dt.minute)) {
                    // Execute job
                    processJob(job);
                }
            }
        }
    }
}

// This function handles the job state machine
// It manages the different states of a job (idle, open valve, start pump, running, stop pump)
// and transitions between these states based on the job's duration and timing
void handleJobStateMachine() {
    switch (currentJobState) {
        case JOB_IDLE:
            // do nothing, wait for a job to start
            break;
        case JOB_OPEN_VALVE: {
            int plantNum = atoi(runningJob.plant + 6);
            handleValveSwitch(plantNum);
            currentJobState = JOB_START_PUMP;
            jobStateTimestamp = millis();
            break;
        }
        case JOB_START_PUMP:
            if (millis() - jobStateTimestamp >= 1000) { // wait 1 second before starting the pump
                handlePumpSwitch();
                currentJobState = JOB_RUNNING;
                jobStateTimestamp = millis();
            }
            break;
        case JOB_RUNNING:
            if (millis() - jobStateTimestamp >= runningJob.duration * 1000) {
                handlePumpSwitch();
                currentJobState = JOB_STOP_PUMP;
                jobStateTimestamp = millis();
            }
            break;
        case JOB_STOP_PUMP:
            if (millis() - jobStateTimestamp >= 5000) { // wait 5 seconds before closing the valve
                int plantNum = atoi(runningJob.plant + 6);
                handleValveSwitch(plantNum);
                currentJobState = JOB_IDLE;
                jobActive = false;
                Serial.println("Job finished.");
                WebSerial.print(F("Job finished.\n"));
            }
            break;
    }
}

void setup(void) {
    initSerial();
    Serial.printf("Application version: %s\n", APP_VERSION);

    initFS();
    initWiFi();

    initWebSocket();

    pinMode(valvePin_1, OUTPUT);
    pinMode(valvePin_2, OUTPUT);
    pinMode(valvePin_3, OUTPUT);
    pinMode(pumpPin, OUTPUT);

    pinMode(moistureSensorPin_1, INPUT);
    pinMode(moistureSensorPin_2, INPUT);
    pinMode(moistureSensorPin_3, INPUT);
    pinMode(soilFlowSensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);

    digitalWrite(valvePin_1, LOW);
    digitalWrite(valvePin_2, LOW);
    digitalWrite(valvePin_3, LOW);
    digitalWrite(pumpPin, LOW);
    delay(500);

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    // Attach Message Callback
    WebSerial.onMessage(recvMsg);

    // Arduino OTA config and start
    ArduinoOTA.setHostname("GrowboxWatering");
    ArduinoOTA.begin();

    // Set HTTP server directives
    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);

    server.serveStatic("/", LittleFS, "/");
    server.serveStatic("/css/", LittleFS, "/css/");
    server.serveStatic("/js/", LittleFS, "/js/");

    //Start HTTP server
    server.begin();
    Serial.println("HTTP server started");
    WebSerial.print(F("HTTP server started\n"));

    // Sync time with NTP server
    syncTime();

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
        // Execute jobs processing
        jobsProcessor();
    }

    // If pump is running calculate runtime and flow rate
    if (pumpState == HIGH) {
        // Update timer
        if ((millis() - lastTime) > timerDelay) {
            // Stopwatch pump runtime
            pumpRunTime = (millis() - pumpStartTime) / 1000;
            // Calculate Soil Flowrate
            calculateSoilFlowRate();
            // Notify clients with updated data
            notifyClients();
            lastTime = millis();
        }
    }

    // Calculate Moisture Sensor Values
    //calculateMoistureSensorValues();

    // Resynchronize with NTP every 30 minutes
    if (millis() - lastNTPUpdate > ntpSyncInterval) {
        syncTime();

        // Current time and date
        Serial.printf("Current time: %02d:%02d:%02d, Date: %04d-%02d-%02d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

        WebSerial.printf("Current time: %02d:%02d:%02d, Date: %04d-%02d-%02d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    }

    delay(1000);
}