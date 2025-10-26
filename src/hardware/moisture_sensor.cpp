#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"

// Moisture sensor constants
const int DRY_ANALOG_VALUE = 4095;  // ESP32 ADC is 12-bit (0-4095), dry soil
const int WET_ANALOG_VALUE = 0;  // Wet soil (adjust based on your sensor calibration)
const int DRY_PERCENT = 0;
const int WET_PERCENT = 100;

std::vector<MoistureSensorData> moistureSensors;

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

    static unsigned long lastDetailedLog = 0;
    unsigned long now = millis();
    bool shouldLogDetails = (now - lastDetailedLog >= 300000); // 5 minutes

    for (size_t i = 0; i < moistureSensors.size(); i++) {
        moistureSensors[i].analogValue = analogRead(moistureSensors[i].pin);
        moistureSensors[i].percentValue = mapMoistureToPercent(moistureSensors[i].analogValue);
        moistureSensors[i].isDry = (moistureSensors[i].percentValue < 20);

        if (shouldLogDetails) {
            logThrottled("Sensor %d (Pin %d): Raw=%d, Moisture=%d%%, Status=%s",
                        i + 1,
                        moistureSensors[i].pin,
                        moistureSensors[i].analogValue,
                        moistureSensors[i].percentValue,
                        moistureSensors[i].isDry ? "DRY" : "OK");
        }
        
        if (moistureSensors[i].isDry) {
            logThrottled("WARNING: Plant %d is dry! Moisture: %d%%", 
                        i + 1, moistureSensors[i].percentValue);
        }
    }
    
    if (shouldLogDetails) {
        lastDetailedLog = now;
    }
}

std::vector<MoistureSensorData> getMoistureSensorData() {
    return moistureSensors;
}