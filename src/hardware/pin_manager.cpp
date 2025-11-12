#include "hardware/pin_manager.h"
#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"

void initializeValvePins() {
    // Clear existing valve data
    valvePins.clear();
    valve_switches.clear();
    valveStates.clear();

    // Log initialization start
    logThrottled("Initializing %d valve(s) starting at pin %d", 
                 settings.plant_count, settings.valve_start_pin);

    // Initialize each valve pin based on plant count
    for (uint8_t i = 0; i < settings.plant_count; i++) {
        int pin = settings.valve_start_pin + i;
        valvePins.push_back(pin);
        valve_switches.push_back(false);
        
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delay(50);
        
        int state = digitalRead(pin);
        valveStates.push_back(state);
        
        logThrottled("Valve %d initialized on pin %d (State: %d)", i + 1, pin, state);
    }
}

void initializeMoisturePins() {
    // Clear existing sensors
    moistureSensors.clear();

    // Check if moisture sensors are enabled
    if (!settings.use_moisturesensor) {
        logThrottled("Moisture sensors disabled in settings");
        return;
    }

    // Log initialization start
    logThrottled("Initializing %d moisture sensor(s) starting at pin %d", 
                 settings.plant_count, settings.moisture_start_pin);

    // Initialize each moisture sensor pin based on plant count
    for (uint8_t i = 0; i < settings.plant_count; i++) {
        MoistureSensorData sensor;
        sensor.pin = settings.moisture_start_pin + i;
        sensor.analogValue = 0;
        sensor.percentValue = 0;
        sensor.isDry = false;
        
        // Read initial value, get DRY_ANALOG_VALUE and WET_ANALOG_VALUE from moisture_sensor.cpp
        int dryAnalogValue = (i == 0) ? DRY_ANALOG_VALUE_0 : (i == 1) ? DRY_ANALOG_VALUE_1 : DRY_ANALOG_VALUE_2;
        int wetAnalogValue = (i == 0) ? WET_ANALOG_VALUE_0 : (i == 1) ? WET_ANALOG_VALUE_1 : WET_ANALOG_VALUE_2;

        // Read initial value
        sensor.analogValue = analogRead(sensor.pin);
        sensor.percentValue = mapMoistureToPercent(sensor.analogValue, wetAnalogValue, dryAnalogValue);
        sensor.isDry = (sensor.percentValue < 20);
        
        // Add to global vector
        moistureSensors.push_back(sensor);

        // Log sensor initialization
        logThrottled("Moisture sensor %d initialized on pin %d (Initial: %d%%, Raw: %d)", 
                     i + 1, sensor.pin, sensor.percentValue, sensor.analogValue);
    }
    
    logThrottled("Total moisture sensors initialized: %d", moistureSensors.size());
}

void initializePumpPin() {
    logThrottled("Initializing pump pin...");
    
    // Initialize pump pin
    pinMode(pumpPin, OUTPUT);
    digitalWrite(pumpPin, LOW);
    logThrottled("Pump pin %d initialized", pumpPin);
}