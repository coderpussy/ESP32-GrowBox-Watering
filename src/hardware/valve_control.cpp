#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "config.h"
#include "utils/logger.h"

void handleValveSwitch(uint8_t valveNum) {
    if (valveNum >= settings.plant_count) {
        logThrottled("Invalid valve number: %d", valveNum);
        return;
    }
    
    bool currentState = valve_switches[valveNum];
    if (currentState) {
        if (pumpCtx.state != PUMP_RUNNING) {
            digitalWrite(valvePins[valveNum], LOW);
            valve_switches[valveNum] = false;
            valveStates[valveNum] = LOW;
            logThrottled("Valve %d closed", valveNum + 1);
        } else {
            logThrottled("Cannot close valve %d - pump is running", valveNum + 1);
        }
    } else {
        digitalWrite(valvePins[valveNum], HIGH);
        valve_switches[valveNum] = true;
        valveStates[valveNum] = HIGH;
        logThrottled("Valve %d opened", valveNum + 1);
    }
}