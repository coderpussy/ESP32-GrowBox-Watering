#include "network/websocket_handler.h"
#include "storage/config_manager.h"
#include "storage/filesystem_manager.h"
#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "hardware/moisture_sensor.h"
#include "hardware/pin_manager.h"
#include "config.h"
#include "utils/logger.h"
#include <ArduinoJson.h>

AsyncWebSocket ws("/ws");

extern const char* configfile;
extern const char* jobsfile;

void notifyClients() {
    handleGetData();
}

void handleGetData() {
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

    JsonArray valveArray = root.createNestedArray("valves");
    for(uint8_t i = 0; i < settings.plant_count && i < valve_switches.size(); i++) {
        JsonObject valve = valveArray.createNestedObject();
        valve["id"] = i + 1;
        valve["state"] = bool(valve_switches[i]);
    }

    serializeJson(root, Text);
    ws.textAll(Text);
}

void handleGetSettings() {
    String Text;
    const uint8_t size = JSON_OBJECT_SIZE(7);
    StaticJsonDocument<size> json;

    json.clear();
    json["action"] = "setsettings";
    json["use_webserial"] = settings.use_webserial;
    json["use_flowsensor"] = settings.use_flowsensor;
    json["use_moisturesensor"] = settings.use_moisturesensor;
    json["auto_switch_enabled"] = settings.auto_switch;
    json["plant_count"] = settings.plant_count;

    serializeJson(json, Text);
    ws.textAll(Text);
}

void handleSaveSettings(const JsonDocument& json) {
    settings.use_webserial = json["use_webserial"] | false;
    settings.use_flowsensor = json["use_flowsensor"] | false;
    settings.use_moisturesensor = json["use_moisturesensor"] | false;
    settings.auto_switch = json["auto_switch_enabled"] | false;
    
    if (settings.auto_switch) auto_switch = settings.auto_switch;
    
    uint8_t new_count = json["plant_count"] | 3;
    if (new_count != settings.plant_count || settings.use_flowsensor || settings.use_moisturesensor) {
        settings.plant_count = new_count;
        initializePins();
        initializeValvePins();
        initializeMoisturePins();
    }

    saveConfiguration(configfile);
    handleGetSettings();
}

void handleGetJobList() {
    int arrayCount = joblistVec.empty() ? 1 : joblistVec.size();
    
    const size_t capacity = JSON_OBJECT_SIZE(2) +
                           JSON_ARRAY_SIZE(arrayCount) +
                           (arrayCount * JSON_OBJECT_SIZE(10)) +
                           (arrayCount * 128);
    
    DynamicJsonDocument doc(capacity);
    doc["action"] = "setjoblist";
    JsonArray joblistArray = doc.createNestedArray("joblist");

    for (size_t i = 0; i < arrayCount && i < joblistVec.size(); i++) {
        const jobStruct& job = joblistVec[i];
        JsonObject obj = joblistArray.createNestedObject();
        
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["type"] = job.type;
        obj["moisture_min"] = job.moisture_min;
        obj["moisture_max"] = job.moisture_max;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }

    String Text;
    serializeJson(doc, Text);
    ws.textAll(Text);
}

void handleAddJobToList(const JsonDocument& json) {
    jobStruct newJob;
    int newId = json["id"] | -1;
    
    if (newId < 0) {
        logThrottled("Invalid job id, skipping");
        return;
    }

    if (newId == 0) {
        logThrottled("First job id, clearing existing job list");
        joblistVec.clear();
    }

    for (const jobStruct& job : joblistVec) {
        if (job.id == newId) {
            logThrottled("Job ID already exists, not adding");
            return;
        }
    }

    newJob.id = newId;
    newJob.active = json["active"] | false;
    strlcpy(newJob.name, json["name"] | "", sizeof(newJob.name));
    int triggerType = json["type"] | 0;
    newJob.type = static_cast<JobTrigger>(triggerType);
    newJob.moisture_min = json["moisture_min"] | 20; // Default 20%
    newJob.moisture_max = json["moisture_max"] | 80; // Default 80%
    newJob.plant = json["plant"] | 0;
    newJob.duration = json["duration"] | 0;
    strlcpy(newJob.starttime, json["starttime"] | "", sizeof(newJob.starttime));
    newJob.everyday = json["everyday"] | false;
    joblistVec.push_back(newJob);

    logThrottled("Added job: %s", newJob.name);
}

void handleGetMoistureSensors() {
    StaticJsonDocument<1024> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");

    std::vector<MoistureSensorData> sensors = getMoistureSensorData();
    
    for (size_t i = 0; i < sensors.size(); i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["id"] = i + 1;
        sensor["pin"] = sensors[i].pin;
        sensor["analog"] = sensors[i].analogValue;
        sensor["percent"] = sensors[i].percentValue;
        sensor["isDry"] = sensors[i].isDry;
    }

    doc["enabled"] = settings.use_moisturesensor;
    doc["count"] = sensors.size();

    String response;
    serializeJson(doc, response);

    ws.textAll(response);
}

void handleAutoSwitch() {
    auto_switch = !auto_switch;
    logThrottled("Auto %s", auto_switch ? "On" : "Off");
    notifyClients();
}

void handleResetCounter() {
    extern volatile int pulseCount;
    extern float soilFlowRate;
    extern float soilFlowVolume;
    extern float roundSoilFlowVolume;
    extern float tempsoilFlowVolume;
    
    pulseCount = 0;
    soilFlowRate = 0.0;
    soilFlowVolume = 0.0;
    roundSoilFlowVolume = 0.0;
    tempsoilFlowVolume = 0.0;
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
            logThrottled("deserializeJson() failed: %s", err.c_str());
            return;
        }

        const char* tmpAct = json["action"] | "";
        String action = String(tmpAct);
        logThrottled("action: %s", action.c_str());

        if (action == "getvalues") handleGetData();
        else if (action == "getsettings") handleGetSettings();
        else if (action == "getjoblist") handleGetJobList();
        else if (action == "savesettings") handleSaveSettings(json);
        else if (action == "addjobtolist") handleAddJobToList(json);
        else if (action == "savejoblist") saveJobList(jobsfile);
        else if (action == "deletejoblist") deleteJobList(jobsfile);
        else if (action == "resetcounter") handleResetCounter();
        else if (action == "getmoisturesensors") handleGetMoistureSensors();
        else if (action == "auto_switch") handleAutoSwitch();
        else if (action == "pump_switch") handlePumpSwitch(true);
        else if (action == "valve_switch") {
            int valveId = json["valve_id"].as<int>();
            if (valveId > 0 && valveId <= settings.plant_count) {
                handleValveSwitch(valveId - 1);
            }
        }
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            logThrottled("WebSocket client #%u connected from %s", 
                client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            logThrottled("WebSocket client #%u disconnected", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_ERROR:
            logThrottled("WebSocket error: client #%u", client->id());
            break;
        case WS_EVT_PONG:
            break;
    }
}

void initWebSocket() {
    ws.onEvent(onWsEvent);
}