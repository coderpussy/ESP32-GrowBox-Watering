#include "hardware/pin_manager.h"
#include "config.h"
#include "utils/logger.h"

void initializePins() {
    // Detach interrupts if previously initialized
    if (settings.use_flowsensor) {
        detachInterrupt(digitalPinToInterrupt(soilFlowSensorPin));
    }

    // Reset existing valve pins
    for(const auto& pin : valvePins) {
        digitalWrite(pin, LOW);
        pinMode(pin, INPUT);
    }

    // Clear and resize vectors
    valvePins.clear();
    valve_switches.clear();
    valveStates.clear();

    valvePins.resize(settings.plant_count);
    valve_switches.resize(settings.plant_count, false);
    valveStates.resize(settings.plant_count, 0);

    // Initialize valve pins
    for(uint8_t i = 0; i < settings.plant_count; i++) {
        valvePins[i] = settings.valve_start_pin + i;
        pinMode(valvePins[i], OUTPUT);
        digitalWrite(valvePins[i], LOW);
        valveStates[i] = digitalRead(valvePins[i]);
    }

    // Initialize pump
    pinMode(pumpPin, OUTPUT);
    digitalWrite(pumpPin, LOW);
    pumpState = digitalRead(pumpPin);

    // Initialize flow sensor if enabled
    if (settings.use_flowsensor) {
        pinMode(soilFlowSensorPin, INPUT_PULLUP);
        // attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);
    }

    logThrottled("Pins initialized for %d plants", settings.plant_count);
}

void cleanupPins() {
    for(const auto& pin : valvePins) {
        digitalWrite(pin, LOW);
    }
    digitalWrite(pumpPin, LOW);
}