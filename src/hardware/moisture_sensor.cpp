#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"

// Moisture sensor constants
const int DRY_ANALOG_VALUE_0 = 4095;  // ESP32 ADC is 12-bit (0-4095), dry soil
const int WET_ANALOG_VALUE_0 = 275;  // Wet soil (adjust based on your sensor calibration)
const int DRY_ANALOG_VALUE_1 = 4095;  // ESP32 ADC is 12-bit (0-4095), dry soil
const int WET_ANALOG_VALUE_1 = 275;  // Wet soil (adjust based on your sensor calibration)
const int DRY_ANALOG_VALUE_2 = 4095;  // ESP32 ADC is 12-bit (0-4095), dry soil
const int WET_ANALOG_VALUE_2 = 275;  // Wet soil (adjust based on your sensor calibration)
const int DRY_PERCENT = 0;
const int WET_PERCENT = 100;

std::vector<MoistureSensorData> moistureSensors;

int mapMoistureToPercent(int analogValue, int wetValue, int dryValue) {
    // Constrain the value to valid range
    analogValue = constrain(analogValue, wetValue, dryValue);

    // Map analog value to percentage (inverted: higher analog = drier = lower percentage)
    return map(analogValue, dryValue, wetValue, DRY_PERCENT, WET_PERCENT);
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
        // Set different DRY_ANALOG_VALUE and WET_ANALOG_VALUE
        int dryAnalogValue = (i == 0) ? DRY_ANALOG_VALUE_0 : (i == 1) ? DRY_ANALOG_VALUE_1 : DRY_ANALOG_VALUE_2;
        int wetAnalogValue = (i == 0) ? WET_ANALOG_VALUE_0 : (i == 1) ? WET_ANALOG_VALUE_1 : WET_ANALOG_VALUE_2;

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
        moistureSensors[i].percentValue = mapMoistureToPercent(moistureSensors[i].analogValue, wetAnalogValue, dryAnalogValue);
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