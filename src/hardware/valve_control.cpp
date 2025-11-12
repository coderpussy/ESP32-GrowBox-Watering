#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "config.h"
#include "utils/logger.h"

void handleValveSwitch(uint8_t valveNum) {
    // Validate valve number
    if (valveNum >= settings.plant_count) {
        logThrottled("Invalid valve number: %d", valveNum);
        return;
    }
    
    // Toggle valve state
    bool currentState = valve_switches[valveNum];

    // If valve is currently open, close it; if closed, open it
    if (currentState) {
        if (pumpCtx.state != PUMP_RUNNING) {
            digitalWrite(valvePins[valveNum], LOW);
            valve_switches[valveNum] = false;
            valveStates[valveNum] = LOW;
            logThrottled("Valve %d closed", valveNum);
        } else {
            logThrottled("Cannot close valve %d - pump is running", valveNum);
        }
    } else {
        digitalWrite(valvePins[valveNum], HIGH);
        valve_switches[valveNum] = true;
        valveStates[valveNum] = HIGH;
        logThrottled("Valve %d opened", valveNum);
    }
}