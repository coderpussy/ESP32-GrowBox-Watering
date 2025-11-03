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
    //analogValue = constrain(analogValue, WET_ANALOG_VALUE, DRY_ANALOG_VALUE);
    
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

    const int numReadings = 10; // Number of readings to average

    for (size_t i = 0; i < moistureSensors.size(); i++) {
        int readings[numReadings];      // the readings from the analog input
        int readIndex = 0;              // the index of the current reading
        int total = 0;                  // the running total
        int average = 0;                // the average

        // take multiple readings to smooth out noise
        for (int thisReading = 0; thisReading < numReadings; thisReading++) {
            readings[thisReading] = analogRead(moistureSensors[i].pin);
            delay(10);  // Small delay between readings
        }

        // Calculate the average
        for (int thisReading = 0; thisReading < numReadings; thisReading++) {
            total += readings[thisReading];
        }
        average = total / numReadings;

        moistureSensors[i].analogValue = average;
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