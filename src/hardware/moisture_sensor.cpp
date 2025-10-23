#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"

// Moisture sensor constants
const int MOISTURE_SENSOR_START_PIN = 16;  // Changed from 16 to avoid conflict with other pins
const int DRY_ANALOG_VALUE = 4095;  // ESP32 ADC is 12-bit (0-4095), dry soil
const int WET_ANALOG_VALUE = 1500;  // Wet soil (adjust based on your sensor calibration)
const int DRY_PERCENT = 0;
const int WET_PERCENT = 100;

std::vector<MoistureSensorData> moistureSensors;

void initMoistureSensors() {
    if (!settings.use_moisturesensor) {
        logThrottled("Moisture sensors disabled in settings");
        return;
    }

    moistureSensors.clear();

    logThrottled("Initializing %d moisture sensor(s) starting at pin %d", 
                 settings.plant_count, MOISTURE_SENSOR_START_PIN);

    for (uint8_t i = 0; i < settings.plant_count; i++) {
        MoistureSensorData sensor;
        sensor.pin = MOISTURE_SENSOR_START_PIN + i;
        sensor.analogValue = 0;
        sensor.percentValue = 0;
        sensor.isDry = false;

        pinMode(sensor.pin, INPUT);
        moistureSensors.push_back(sensor);

        logThrottled("Moisture sensor %d initialized on pin %d", i + 1, sensor.pin);
    }
}

int mapMoistureToPercent(int analogValue) {
    // Constrain the value to valid range
    analogValue = constrain(analogValue, WET_ANALOG_VALUE, DRY_ANALOG_VALUE);
    
    // Map analog value to percentage (inverted: higher analog = drier = lower percentage)
    return map(analogValue, DRY_ANALOG_VALUE, WET_ANALOG_VALUE, DRY_PERCENT, WET_PERCENT);
}

void readMoistureSensors() {
    if (!settings.use_moisturesensor || moistureSensors.empty()) {
        return;
    }

    for (size_t i = 0; i < moistureSensors.size(); i++) {
        // Read analog value
        moistureSensors[i].analogValue = analogRead(moistureSensors[i].pin);
        
        // Convert to percentage
        moistureSensors[i].percentValue = mapMoistureToPercent(moistureSensors[i].analogValue);
        
        // Check if dry (below 20%)
        moistureSensors[i].isDry = (moistureSensors[i].percentValue < 20);

        // Log with throttling
        static unsigned long lastDetailedLog = 0;
        unsigned long now = millis();
        
        // Detailed log every 5 minutes
        if (now - lastDetailedLog >= 300000) {
            logThrottled("Sensor %d (Pin %d): Raw=%d, Moisture=%d%%, Status=%s",
                        i + 1,
                        moistureSensors[i].pin,
                        moistureSensors[i].analogValue,
                        moistureSensors[i].percentValue,
                        moistureSensors[i].isDry ? "DRY" : "OK");
        }
        
        // Always log when sensor becomes dry
        if (moistureSensors[i].isDry) {
            logThrottled("WARNING: Plant %d is dry! Moisture: %d%%", 
                        i + 1, moistureSensors[i].percentValue);
        }
        
        if (now - lastDetailedLog >= 300000) {
            lastDetailedLog = now;
        }
    }
}

std::vector<MoistureSensorData> getMoistureSensorData() {
    return moistureSensors;
}