#include "hardware/pump_control.h"
#include "config.h"
#include "utils/logger.h"

void handlePumpSwitch(bool manual) {
    unsigned long now = millis();
    bool stateChanged = false;
    bool anyValveOpen = false;

    if (manual) {
        pumpCtx.manualControl = true;
        pumpCtx.targetState = !pump_switch;
        
        if (!pumpCtx.targetState) {
            pumpCtx.state = PUMP_STOPPING;
        } 
        else if (pumpCtx.state == PUMP_IDLE) {
            pumpCtx.state = PUMP_STARTING;
        }
        pumpCtx.stateTime = now;
    }

    switch (pumpCtx.state) {
        case PUMP_IDLE:
            break;

        case PUMP_STARTING:
            anyValveOpen = false;
            for(uint8_t i = 0; i < settings.plant_count; i++) {
                if (valveStates[i] == HIGH) {
                    anyValveOpen = true;
                    break;
                }
            }

            if (anyValveOpen) {
                digitalWrite(pumpPin, HIGH);
                pump_switch = true;
                pumpCtx.state = PUMP_RUNNING;
                pumpStartMillis = now;
                logThrottled("Pump starting %s", pumpCtx.manualControl ? "(manual)" : "(auto)");
                stateChanged = true;
            } else {
                logThrottled("Cannot start pump - no valve open");
                pumpCtx.state = PUMP_IDLE;
                pump_switch = false;
                stateChanged = true;
            }
            break;

        case PUMP_RUNNING:
            if (!pumpCtx.targetState) {
                pumpCtx.state = PUMP_STOPPING;
                pumpCtx.stateTime = now;
            }
            pumpRunTime = (now - pumpStartMillis) / 1000.0f;
            break;

        case PUMP_STOPPING:
            digitalWrite(pumpPin, LOW);
            pump_switch = false;
            pumpCtx.state = PUMP_IDLE;
            pumpRunTime = (now - pumpStartMillis) / 1000.0f;
            logThrottled("Pump stopping %s after %.1f seconds", 
                pumpCtx.manualControl ? "(manual)" : "(auto)",
                pumpRunTime);
            stateChanged = true;
            break;
    }

    if (stateChanged) {
        pumpState = digitalRead(pumpPin);
        if (!pump_switch) {
            pumpRunTime = 0;
            pumpStartMillis = 0;
        }
    }
}