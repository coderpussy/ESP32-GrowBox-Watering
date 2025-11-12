#include "hardware/flow_sensor.h"
#include "hardware/pin_manager.h"
#include "config.h"
#include "utils/logger.h"

// Flow sensor calibration constant for YF-S201
const float FLOW_CALIBRATION_FACTOR = 18;  // pulses per second per L/min

volatile int pulseCount = 0;
float soilFlowRate = 0.0;          // Flow rate in L/min
float soilFlowVolume = 0.0;        // Total volume in L

static unsigned long lastFlowTime = 0;

void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

void initFlowSensorPin() {
    pinMode(soilFlowSensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(soilFlowSensorPin), pulseCounter, FALLING);
    
    pulseCount = 0;
    soilFlowRate = 0.0;
    soilFlowVolume = 0.0;
    lastFlowTime = millis();
    
    logThrottled("Flow sensor initialized on pin %d", soilFlowSensorPin);
}

void calculateSoilFlowRate() {
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastFlowTime;
    
    if (deltaTime == 0) return;  // Prevent division by zero
    
    // Atomically read and reset pulse count
    noInterrupts();
    int currentPulseCount = pulseCount;
    pulseCount = 0;
    interrupts();
    
    // Calculate flow rate in L/min
    // Formula: (pulses per second) / calibration factor
    soilFlowRate = ((1000.0 / deltaTime) * currentPulseCount) / FLOW_CALIBRATION_FACTOR;
    
    // Add volume increment (convert from L/min to L)
    float volumeIncrement = (soilFlowRate / 60.0) * (deltaTime / 100.0);
    //float volumeIncrement = (soilFlowRate / 60.0) * 1000.0;
    //float volumeIncrement = soilFlowRate * 1000.0 / 60.0;
    soilFlowVolume += volumeIncrement;
    
    // Update last flow time
    lastFlowTime = currentTime;
}

void resetFlowSensor() {
    noInterrupts();
    pulseCount = 0;
    interrupts();
    
    soilFlowRate = 0.0;
    soilFlowVolume = 0.0;
    lastFlowTime = millis();
    
    logThrottled("Flow sensor reset");
}