#include "scheduler/job_state_machine.h"
#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "utils/logger.h"
#include "network/websocket_handler.h"

void processJob(const jobStruct& job) {
    if (jobActive) {
        logThrottled("Another job is active, skipping start");
        return;
    }
    
    runningJob = job;
    currentJobState = JOB_OPEN_VALVE;
    jobStateTimestamp = millis();
    jobActive = true;
    logThrottled("Start background job: %s for plant: %s", job.type, job.plant);
}

void handleJobStateMachine() {
    if (!jobActive) return;

    unsigned long now = millis();
    switch (currentJobState) {
        case JOB_IDLE:
            break;
            
        case JOB_OPEN_VALVE: {
            uint8_t plantNum = runningJob.plant;
            //sscanf(runningJob.plant, "plant-%d", &plantNum);
            
            if (plantNum >= 0 && plantNum < settings.plant_count) {
                handleValveSwitch(plantNum);
                currentJobState = JOB_START_PUMP;
                jobStateTimestamp = now;
            } else {
                logThrottled("Invalid plant number in job - aborting");
                jobActive = false;
                currentJobState = JOB_IDLE;
            }
            break;
        }
        
        case JOB_START_PUMP:
            if (now - jobStateTimestamp >= 500) {
                pumpCtx.manualControl = false;
                pumpCtx.targetState = true;
                pumpCtx.state = PUMP_STARTING;
                handlePumpSwitch(false);
                
                if (pumpCtx.state == PUMP_RUNNING) {
                    logThrottled("Pump started for job: %s", runningJob.name);
                    currentJobState = JOB_RUNNING;
                    jobStateTimestamp = now;
                } else {
                    logThrottled("Failed to start pump for job - aborting");
                    jobActive = false;
                    currentJobState = JOB_IDLE;
                }
            }
            break;
            
        case JOB_RUNNING:
            if (now - jobStateTimestamp >= (unsigned long)runningJob.duration * 1000UL) {
                pumpCtx.manualControl = false;
                pumpCtx.targetState = false;
                pumpCtx.state = PUMP_STOPPING;
                handlePumpSwitch(false);
                currentJobState = JOB_STOP_PUMP;
                jobStateTimestamp = now;
                logThrottled("Job duration complete, stopping pump");
            }
            break;
            
        case JOB_STOP_PUMP:
            if (now - jobStateTimestamp >= 750) {
                uint8_t plantNum = runningJob.plant;
                //sscanf(runningJob.plant, "plant-%d", &plantNum);
                
                if (plantNum >= 0 && plantNum < settings.plant_count) {
                    handleValveSwitch(plantNum);
                    currentJobState = JOB_CLOSE_VALVE;
                    jobStateTimestamp = now;
                } else {
                    logThrottled("Invalid plant number %d - aborting", plantNum + 1);
                    jobActive = false;
                    currentJobState = JOB_IDLE;
                }
            }
            break;
            
        case JOB_CLOSE_VALVE:
            jobActive = false;
            currentJobState = JOB_IDLE;
            logThrottled("Job finished.");
            notifyClients();
            break;
    }
}