#include "storage/config_manager.h"
#include "storage/filesystem_manager.h"
#include "hardware/pin_manager.h"
#include "config.h"
#include "utils/logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

void loadConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        File file = LittleFS.open(configfile, "r");
        
        const size_t capacity = JSON_OBJECT_SIZE(6) + 128;
        DynamicJsonDocument doc(capacity);
        
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            logThrottled("Failed to read configuration file: %s", error.c_str());
            return;
        }

        settings.use_webserial = doc["use_webserial"] | false;
        settings.use_flowsensor = doc["use_flowsensor"] | false;
        settings.use_moisturesensor = doc["use_moisturesensor"] | false;
        settings.auto_switch = doc["auto_switch_enabled"] | false;
        settings.plant_count = doc["plant_count"] | 3;

        if (settings.auto_switch) auto_switch = settings.auto_switch;

        logThrottled("Configuration loaded - Plants: %d, AutoSwitch: %d",
            settings.plant_count, settings.auto_switch);
    } else {
        logThrottled("No configuration file found, using defaults");
    }
}

void saveConfiguration(const char* configfile) {
    if (LittleFS.exists(configfile)) {
        LittleFS.remove(configfile);
    }

    File file = LittleFS.open(configfile, "w");
    if (!file) {
        logThrottled("Failed to create configuration file");
        return;
    }

    const uint8_t size = JSON_OBJECT_SIZE(6);
    StaticJsonDocument<size> doc;

    doc["use_webserial"] = settings.use_webserial;
    doc["use_flowsensor"] = settings.use_flowsensor;
    doc["use_moisturesensor"] = settings.use_moisturesensor;
    doc["auto_switch_enabled"] = settings.auto_switch;
    doc["plant_count"] = settings.plant_count;

    if (serializeJson(doc, file) == 0) {
        logThrottled("Failed to write to configuration file");
    }

    file.close();
    printFile(configfile);
}