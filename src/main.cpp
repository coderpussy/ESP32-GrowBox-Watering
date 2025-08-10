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

// ESP32 Grow Irrigation System
// Controls water pump and water valve units for 3 plants in a grow tent

// History
// Version 0.9 pre2, 18.07.2025, coderpussy
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

#include <vector> // Für dynamisches Array

// Get the number of elements in an array 
#define ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

// Time that the daily task runs in 24 hour format
//const int taskHour = 16;   // Hour example in 24 hour format: 16 = 4 PM
//const int taskMinute = 5;  // 5 minutes

// Store the day when the task last ran to ensure it only runs once per day
//int lastRunDay = -1;

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
// Initialize empty joblist structure
// e.g. /*{1,false,"name","job","plant",90,"starttime",false},
jobStruct joblist[] = {};
std::vector<jobStruct> joblistVec; // Dynamisches Array für Jobs

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

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
        const uint8_t size = JSON_OBJECT_SIZE(10);
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

            /*
            float tmp = 0;

            // Copy values from the JsonDocument to the Config
            tmp = doc["Astra_Az"];
            if ((tmp >= 90) && (tmp <= 270)) Astra_Az = tmp;

            tmp = doc["Astra_El"];
            if ((tmp >= 10) && (tmp <= 50)) Astra_El = tmp;

            tmp = doc["El_Offset"];
            if (abs(tmp) <= 90) El_Offset = tmp;

            tmp = doc["Az_Offset"];
            if (abs(tmp) <= 90) Az_Offset = tmp;

            tmp = doc["motorSpeed"];
            if ((tmp >= 500) && (tmp < 1024)) motorSpeed = trunc(tmp);
            */
        }
    }
}

// Saves the configuration to a file
void saveConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        // Delete existing file, otherwise the configuration is appended to the file
        LittleFS.remove(configfile);
    }

    // Open file for writing
    File file = LittleFS.open(configfile, "w");
    if (!file) {
        Serial.println(F("Failed to create configuration file"));
        WebSerial.print(F("Failed to create configuration file\n"));
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const uint8_t size = JSON_OBJECT_SIZE(10);
    StaticJsonDocument<size> doc;

    // Set the values in the document
    /*
    doc["Azimut"] = Astra_Az;
    doc["Elevator"] = Astra_El;
    doc["El_Offset"] = El_Offset;
    doc["Az_Offset"] = Az_Offset;
    doc["motorSpeed"] = motorSpeed;
    */

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to configuration file"));
        WebSerial.print(F("Failed to write to configuration file\n"));
    }

    // Close the file
    file.close();
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
void loadJobsDynamic(const char* jobsfile) {
    int jobCount = countJsonObjectsInFile(jobsfile);

    if (jobCount == 0) {
        Serial.println(F("No jobs found or file error"));
        return;
    }

    // Passe die Felderzahl ggf. an deine Struktur an!
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

        // Annahme: Die Datei enthält ein JSON-Array auf oberster Ebene
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

        //Serial.printf("Loaded %d jobs\n", joblistVec.size());
        jobStruct joblist[jobCount];

        int count = std::min((int)joblistVec.size(), jobCount);
        for (int i = 0; i < count; ++i) {
            joblist[i] = joblistVec[i];
        }
    }
}

// Saves the job schedules to a file
void handleSaveJobList(const char* jobsfile) {
    if (LittleFS.exists(jobsfile)) {
        // Delete existing file, otherwise the configuration is appended to the file
        LittleFS.remove(jobsfile);
    }

    // Open file for writing
    File file = LittleFS.open(jobsfile, "w");
    if (!file) {
        Serial.println(F("Failed to create job schedules file"));
        WebSerial.print(F("Failed to create job schedules file\n"));
        return;
    }

    // Get joblist array length
    const int joblen = ARRAY_LEN(joblist);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    const int size = JSON_ARRAY_SIZE(joblen) + joblen*JSON_OBJECT_SIZE(9);
    StaticJsonDocument<size> doc;

    // Set the values in the document
    for (jobStruct& s: joblist) {
        JsonObject obj = doc.createNestedObject();
        obj["id"] = s.id;
        obj["active"] = s.active;
        obj["name"] =s.name;
        obj["job"] =s.job;
        obj["plant"] =s.plant;
        obj["duration"] =s.duration;
        obj["starttime"] =s.starttime;
        obj["everyday"] =s.everyday;
    }

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to job schedules file"));
        WebSerial.print(F("Failed to write to job schedules file\n"));
    }

    // Close the file
    file.close();

    // Debug: Print file data
    printFile(jobsfile);

    // Serialize JSON object to string
    /*String jsonString;
    serializeJson(doc, jsonString);
    Serial.println(jsonString);
    WebSerial.println(jsonString);*/
}

void handleGetData() {
    String Text;

    const uint8_t size = JSON_OBJECT_SIZE(9);
    StaticJsonDocument<size> root;

    root["action"] = "getvalues";

    root["auto_switch"] = auto_switch;
    root["valve_switch_1"] = valve_switch_1;
    root["valve_switch_2"] = valve_switch_2;
    root["valve_switch_3"] = valve_switch_3;

    root["pump_switch"] = pump_switch;
    root["pumpRunTime"] = String(pumpRunTime, 2);

    root["soilFlowVolume"] = String(roundSoilFlowVolume, 3);

    serializeJson(root, Text);

    ws.textAll(Text); //Send sensors values to websocket clients
}

void handleGetSettings() {
    String Text;

    const uint8_t size = JSON_OBJECT_SIZE(7);
    StaticJsonDocument<size> root;

    root["action"] = action;
    /*
    root["azimut"] = Astra_Az;
    root["elevation"] = Astra_El;
    root["el_offset"] = El_Offset;
    root["az_offset"] = Az_Offset;
    root["motor_speed"] = motorSpeed;
    */

    serializeJson(root, Text);

    ws.textAll(Text); //Send values to websocket clients 
}

void handleSetSettings(const JsonDocument& json) {
    //float tmp;
    String Text;

    /*
    tmp = float(json["azimut"]);
    if ((tmp >= 90) && (tmp <= 270)) Astra_Az = tmp;

    tmp = float(json["elevation"]);
    if ((tmp >= 10) && (tmp <= 50)) Astra_El = tmp;

    tmp = float(json["el_offset"]);
    if (abs(tmp) <= 90) El_Offset = tmp;

    tmp = float(json["az_offset"]);
    if (abs(tmp) <= 90) Az_Offset = tmp;

    tmp = float(json["motor_speed"]);
    if ((tmp >= 500) && (tmp < 1024)) motorSpeed = trunc(tmp);
    */

    saveConfiguration(configfile);

    serializeJson(json, Text);

    ws.textAll(Text);
}

void handleAddJobToList(const JsonDocument& json) {
    //float tmp;
    //String Text;
    
    // Get next joblist index
    int job = ARRAY_LEN(joblist);

    joblist[job] = {json["id"],json["active"],json["name"],json["job"],json["plant"],json["duration"],json["starttime"],json["everyday"]};
    
    /*serializeJson(json, Text);

    Serial.println(Text);
    WebSerial.println(Text);

    ws.textAll(Text);*/
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

void handleValveSwitch1() {
    if (valve_switch_1) {
        // Close valve only if pump isn't running
        if (pumpState == LOW) {
            // Close magnetic valve
            digitalWrite(valvePin_1, LOW);
            valve_switch_1 = false;
            Serial.println("Valve Switch 1 closed");
            WebSerial.print(F("Valve Switch 1 closed\n"));
        }
    } else {
        // Open magnetic valve
        digitalWrite(valvePin_1, HIGH);
        valve_switch_1 = true;
        Serial.println("Valve Switch 1 open");
        WebSerial.print(F("Valve Switch 1 open\n"));
    }

    // Read the state of the valve 1 pin value
    valve1State = digitalRead(valvePin_1);

    notifyClients();
}

void handleValveSwitch2() {
    if (valve_switch_2) {
        // Close valve only if pump isn't running
        if (pumpState == LOW) {
            // Close magnetic valve
            digitalWrite(valvePin_2, LOW);
            valve_switch_2 = false;
            Serial.println("Valve Switch 2 closed");
            WebSerial.print(F("Valve Switch 2 closed\n"));
        }
    } else {
        // Open magnetic valve
        digitalWrite(valvePin_2, HIGH);
        valve_switch_2 = true;
        Serial.println("Valve Switch 2 open");
        WebSerial.print(F("Valve Switch 2 open\n"));
    }

    // Read the state of the valve 2 pin value
    valve2State = digitalRead(valvePin_2);

    notifyClients();
}

void handleValveSwitch3() {
    if (valve_switch_3) {
        // Close valve only if pump isn't running
        if (pumpState == LOW) {
            // Close magnetic valve
            digitalWrite(valvePin_3, LOW);
            valve_switch_3 = false;
            Serial.println("Valve Switch 3 closed");
            WebSerial.print(F("Valve Switch 3 closed\n"));
        }
    } else {
        // Open magnetic valve
        digitalWrite(valvePin_3, HIGH);
        valve_switch_3 = true;
        Serial.println("Valve Switch 3 open");
        WebSerial.print(F("Valve Switch 3 open\n"));
    }

    // Read the state of the valve 3 pin value
    valve3State = digitalRead(valvePin_3);

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
        else if (action == "setsettings") handleSetSettings(json);
        else if (action == "addjobtolist") handleAddJobToList(json);
        else if (action == "savejoblist") handleSaveJobList(jobsfile);
        else if (action == "resetcounter") handleResetCounter();
        else if (action == "auto_switch") handleAutoSwitch();
        else if (action == "valve_switch_1") handleValveSwitch1();
        else if (action == "valve_switch_2") handleValveSwitch2();
        else if (action == "valve_switch_3") handleValveSwitch3();
        else if (action == "pump_switch") handlePumpSwitch();

        /*    else if (action == "on") handleOn();
        else if (action == "off") handleOff();
        else if (action == "r_off") handleRotorOff();
        else if (action == "az_up") handleAzUp();
        else if (action == "az_down") handleAzDown();
        else if (action == "el_up") handleElUp();
        else if (action == "el_down") handleElDown();
        else if (action == "om_el_up") handleOmElUp(json);
        else if (action == "om_el_down") handleOmElDown(json);
        else if (action == "rotor_up") handleRotorUp();
        else if (action == "rotor_down") handleRotorDown();
        else if (action == "rotor_up_step") handleRotorUpStep();
        else if (action == "rotor_down_step") handleRotorDownStep();
        else if (action == "cal") handleCal();
        else if (action == "slider1") handleSlider1(json);
        else if (action == "slider2") handleSlider2(json);
        else if (action == "slider3") handleSlider3(json);
        */
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
  
void calculateMoistureSensorValues() {
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
}

/*void dailyTask() {
    Serial.println("#########\nDoing daily task...\n#########");
    WebSerial.print(F("#########\nDoing daily task...\n#########"));
    // ENTER YOUR TASK HERE
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
    //loadConfiguration(configfile);

    // Load JSON job schedules
    //loadJobs(jobsfile);
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

    // Check if it's time to run the daily task
    /*if (timeinfo.tm_hour == taskHour && timeinfo.tm_min == taskMinute && lastRunDay != timeinfo.tm_mday) {
        dailyTask();
        // Set the day to ensure it only runs once per day
        lastRunDay = timeinfo.tm_mday;
    }*/

    
    // Start tasks if pump is running
    if (pumpState == HIGH) {
        // Update timer
        if ((millis() - lastTime) > timerDelay) {
            // Stopwatch pump runtime
            pumpRunTime = (millis() - pumpStartTime) / 1000;
            
            // Calculate Soil Flowrate
            calculateSoilFlowRate();

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

    //Serial.println();
    delay(1000);
    //delay(10);
}
