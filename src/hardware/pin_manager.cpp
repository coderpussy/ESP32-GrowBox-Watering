#include "hardware/pin_manager.h"
#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"

void initializeValvePins() {
    valvePins.clear();
    valve_switches.clear();
    valveStates.clear();

    logThrottled("Initializing %d valve(s) starting at pin %d", 
                 settings.plant_count, settings.valve_start_pin);

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
    moistureSensors.clear();

    if (!settings.use_moisturesensor) {
        logThrottled("Moisture sensors disabled in settings");
        return;
    }

    logThrottled("Initializing %d moisture sensor(s) starting at pin %d", 
                 settings.plant_count, settings.moisture_start_pin);

    for (uint8_t i = 0; i < settings.plant_count; i++) {
        MoistureSensorData sensor;
        sensor.pin = settings.moisture_start_pin + i;
        sensor.analogValue = 0;
        sensor.percentValue = 0;
        sensor.isDry = false;

        //pinMode(sensor.pin, INPUT);
        
        // Read initial value
        sensor.analogValue = analogRead(sensor.pin);
        sensor.percentValue = mapMoistureToPercent(sensor.analogValue);
        sensor.isDry = (sensor.percentValue < 20);
        
        moistureSensors.push_back(sensor);

        logThrottled("Moisture sensor %d initialized on pin %d (Initial: %d%%, Raw: %d)", 
                     i + 1, sensor.pin, sensor.percentValue, sensor.analogValue);
    }
    
    logThrottled("Total moisture sensors initialized: %d", moistureSensors.size());
}

void initializePins() {
    logThrottled("Initializing hardware pins...");
    
    // Initialize pump and flow sensor pins
    pinMode(pumpPin, OUTPUT);
    pinMode(soilFlowSensorPin, INPUT_PULLUP);
    digitalWrite(pumpPin, LOW);
    
    logThrottled("Pump pin %d and flow sensor pin %d initialized", pumpPin, soilFlowSensorPin);
}