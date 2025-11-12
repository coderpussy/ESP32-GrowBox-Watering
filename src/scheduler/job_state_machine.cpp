#include "scheduler/job_state_machine.h"
#include "hardware/valve_control.h"
#include "hardware/pump_control.h"
#include "hardware/moisture_sensor.h"
#include "utils/logger.h"
#include "network/websocket_handler.h"
#include "config.h"

extern float soilFlowVolume;  // Current total volume from flow sensor
extern float jobStartVolume;  // Volume when job started

// Helper to determine job control type
JobControlType getJobControlType(const jobStruct& job) {
    switch (job.type) {
        case 0:
            return CONTROL_TIME_VOLUME;
        case 1:
            return CONTROL_MOISTURE;
        case 2:
            return CONTROL_MOISTURE_TIME_VOLUME;
        default:
            // Log warning for debugging
            logThrottled("Warning: Invalid job type %d, defaulting to time/volume", job.type);
            return CONTROL_TIME_VOLUME;
    }
}

// Helper function to check time/volume based completion
bool checkTimeVolumeCompletion(const jobStruct& job, unsigned long now, unsigned long jobStateTimestamp, 
                               bool& shouldStop, const char*& stopReason) {
    // Check volume-based completion
    if (job.volume > 0) {
        if (settings.use_flowsensor) {
            float volumeDispensed = soilFlowVolume - jobStartVolume;
            
            // Log progress every 5 seconds
            static unsigned long lastVolumeLog = 0;
            if (now - lastVolumeLog >= 5000) {
                logThrottled("Volume progress: %.2f/%.2fL (%.1f%%)", 
                           volumeDispensed, 
                           job.volume,
                           (volumeDispensed / job.volume) * 100.0f);
                lastVolumeLog = now;
            }
            
            // Check if target volume reached
            if (volumeDispensed >= job.volume) {
                shouldStop = true;
                stopReason = "target volume reached";
                return true;
            }
        } else {
            // Flow sensor not enabled - fallback to time-based
            logThrottled("WARNING: Volume-based job but flow sensor disabled - using duration fallback");
            if (job.duration > 0 && 
                (now - jobStateTimestamp >= (unsigned long)(job.duration * 1000.0f))) {
                shouldStop = true;
                stopReason = "duration fallback (no flow sensor)";
                return true;
            }
        }
    }
    // Check time-based completion
    else if (job.duration > 0) {
        // Check if duration elapsed
        if (now - jobStateTimestamp >= (unsigned long)(job.duration * 1000.0f)) {
            shouldStop = true;
            stopReason = "duration complete";
            return true;
        }
    }
    
    return false;
}

void processJob(const jobStruct& job) {
    // Check if another job is active
    if (jobActive) {
        logThrottled("Another job is active, skipping start");
        return;
    }
    
    // Initialize job state machine
    runningJob = job;
    currentJobState = JOB_VALVE_OPENING;
    jobStateTimestamp = millis();
    jobActive = true;
    // Reset volume tracking
    jobStartVolume = 0.0f;
    
    // Log job type
    JobControlType controlType = getJobControlType(job);
    
    switch (controlType) {
        case CONTROL_TIME_VOLUME:
            // Date Time or Volume based control
            if (job.duration > 0) {
                logThrottled("Start date time-based job: valve %d, duration %.1fs", 
                             job.plant, job.duration);
            }
            if (job.volume > 0) {
                logThrottled("Start date volume-based job: valve %d, target %.2fL", 
                             job.plant, job.volume);
            }
            break;
        case CONTROL_MOISTURE:
            logThrottled("Start moisture-based job: valve %d, min %d%% -> max %d%%", 
                         job.plant, job.moisture_min, job.moisture_max);
            break;
        case CONTROL_MOISTURE_TIME_VOLUME:
            // Moisture Min with Time or Volume based control
            if (job.duration > 0) {
                logThrottled("Start moisture-time-based job: valve %d, duration %.1fs, min %d%%", 
                            job.plant, job.duration, job.moisture_min);
            }
            if (job.volume > 0) {
                logThrottled("Start moisture-volume-based job: valve %d, target %.2fL, min %d%%", 
                            job.plant, job.volume, job.moisture_min);
            }
            break;
    }
}

void handleJobStateMachine() {
    // If no job is active, nothing to do
    if (!jobActive) return;

    // Get current time in milliseconds
    unsigned long now = millis();

    // Handle job state transitions
    switch (currentJobState) {
        case JOB_IDLE:
            // Should not reach here if jobActive is true
            break;
            
        case JOB_VALVE_OPENING: {
            // Open the valve for the specified plant
            uint8_t plantNum = runningJob.plant;
            
            // Only open valve if plant number is valid
            if (plantNum >= 0 && plantNum <= settings.plant_count) {
                handleValveSwitch(plantNum);
                currentJobState = JOB_PUMP_STARTING;
                jobStateTimestamp = now;
            } else {
                logThrottled("Invalid plant number in job - aborting");
                jobActive = false;
                currentJobState = JOB_IDLE;
            }
            break;
        }

        case JOB_PUMP_STARTING: {
            // Start the pump after a short delay to allow valve to open
            if (now - jobStateTimestamp >= 500) {
                // Set pump context and start pump
                pumpCtx.manualControl = false;
                pumpCtx.targetState = true;
                pumpCtx.state = PUMP_STARTING;
                handlePumpSwitch(false);

                // Wait for pump to reach running state
                if (pumpCtx.state == PUMP_RUNNING) {
                    // Record starting volume for volume-based jobs
                    if (runningJob.volume > 0 && settings.use_flowsensor) {
                        jobStartVolume = soilFlowVolume;
                        logThrottled("Job started - initial volume: %.2fL", jobStartVolume);
                    }

                    // Record starting moisture for moisture-based jobs
                    if (runningJob.moisture_max > 0 && settings.use_moisturesensor) {
                        if (runningJob.plant < moistureSensors.size()) {
                            uint8_t startMoisture = moistureSensors[runningJob.plant].percentValue;
                            logThrottled("Job started - initial moisture: %d%%", startMoisture);
                        }
                    }

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
        }
            
        case JOB_RUNNING: {
            bool shouldStop = false;
            const char* stopReason = "";
            
            // Determine job control type
            JobControlType controlType = getJobControlType(runningJob);

            // Handle Date time/volume-based control (both CONTROL_TIME_VOLUME and CONTROL_MOISTURE_TIME_VOLUME)
            if (controlType == CONTROL_TIME_VOLUME || controlType == CONTROL_MOISTURE_TIME_VOLUME) {
                checkTimeVolumeCompletion(runningJob, now, jobStateTimestamp, shouldStop, stopReason);
            }
            // Moisture-based control
            else if (controlType == CONTROL_MOISTURE) {
                if (settings.use_moisturesensor) {
                    if (runningJob.plant <= moistureSensors.size()) {
                        // Read current moisture every loop iteration
                        readMoistureSensors();
                        uint8_t currentMoisture = moistureSensors[runningJob.plant].percentValue;

                        // Log progress every 5 seconds
                        static unsigned long lastMoistureLog = 0;
                        if (now - lastMoistureLog >= 5000) {
                            logThrottled("Moisture progress: %d%% (target: %d%%)", 
                                       currentMoisture, runningJob.moisture_max);
                            lastMoistureLog = now;
                        }
                        
                        // Stop when moisture reaches or exceeds max threshold
                        if (currentMoisture >= runningJob.moisture_max) {
                            shouldStop = true;
                            stopReason = "moisture target reached";
                            logThrottled("Moisture reached %d%% (target: %d%%)", 
                                       currentMoisture, runningJob.moisture_max);
                        }
                        
                        // Safety: If moisture hasn't increased after 5 minutes, stop
                        static uint8_t lastMoistureValue = 0;
                        static unsigned long moistureStuckTime = 0;
                        
                        if (currentMoisture <= lastMoistureValue) {
                            if (moistureStuckTime == 0) {
                                moistureStuckTime = now;
                            } else if (now - moistureStuckTime >= 300000) { // 5 minutes
                                shouldStop = true;
                                stopReason = "moisture not increasing (sensor issue?)";
                                logThrottled("WARNING: Moisture stuck at %d%% for 5 minutes", currentMoisture);
                            }
                        } else {
                            moistureStuckTime = 0; // Reset if moisture is increasing
                        }
                        lastMoistureValue = currentMoisture;
                        
                    } else {
                        logThrottled("ERROR: Invalid sensor index for moisture-based job - aborting");
                        shouldStop = true;
                        stopReason = "sensor error";
                    }
                } else {
                    // Moisture sensor not enabled - fallback to time-based
                    logThrottled("WARNING: Moisture-based job but sensor disabled - using duration fallback");
                    if (runningJob.duration > 0 && 
                        (now - jobStateTimestamp >= (unsigned long)(runningJob.duration * 1000.0f))) {
                        shouldStop = true;
                        stopReason = "duration fallback (no moisture sensor)";
                    }
                }
            }
            
            // Safety timeout - maximum 10 minutes for any job
            if (now - jobStateTimestamp >= 600000) {
                shouldStop = true;
                stopReason = "safety timeout (10 min)";
                logThrottled("WARNING: Job safety timeout reached!");
            }

            // Handle job stopping
            if (shouldStop) {
                // Log final job stats
                if (runningJob.volume > 0 && settings.use_flowsensor) {
                    float finalVolume = soilFlowVolume - jobStartVolume;
                    logThrottled("Job stopping - %s: %.2fL dispensed", stopReason, finalVolume);
                } else if (runningJob.moisture_max > 0 && settings.use_moisturesensor) {
                    if (runningJob.plant < moistureSensors.size()) {
                        uint8_t finalMoisture = moistureSensors[runningJob.plant].percentValue;
                        logThrottled("Job stopping - %s: moisture at %d%%", stopReason, finalMoisture);
                    }
                } else {
                    logThrottled("Job stopping - %s", stopReason);
                }

                // Stop the pump
                pumpCtx.manualControl = false;
                pumpCtx.targetState = false;
                pumpCtx.state = PUMP_STOPPING;
                handlePumpSwitch(false);
                currentJobState = JOB_PUMP_STOPPING;
                jobStateTimestamp = now;
                logThrottled("Job complete, stopping pump");
            }
            break;
        }
            
        case JOB_PUMP_STOPPING: {
            // Wait for pump to stop
            if (now - jobStateTimestamp >= 750) {
                uint8_t plantNum = runningJob.plant;
                
                if (plantNum >= 0 && plantNum <= settings.plant_count) {
                    handleValveSwitch(plantNum);
                    currentJobState = JOB_VALVE_CLOSING;
                    jobStateTimestamp = now;
                } else {
                    logThrottled("Invalid plant number %d - aborting", plantNum);
                    jobActive = false;
                    currentJobState = JOB_IDLE;
                }
            }
            break;
        }
            
        case JOB_VALVE_CLOSING: {
            jobActive = false;
            currentJobState = JOB_IDLE;
            
            // Final summary
            if (runningJob.volume > 0 && settings.use_flowsensor) {
                float totalVolume = soilFlowVolume - jobStartVolume;
                logThrottled("Job finished - Total volume: %.2fL (target: %.2fL)", 
                            totalVolume, runningJob.volume);
            } else if (runningJob.moisture_max > 0 && settings.use_moisturesensor) {
                    if (runningJob.plant < moistureSensors.size()) {
                        uint8_t finalMoisture = moistureSensors[runningJob.plant].percentValue;
                        logThrottled("Job finished - Final moisture: %d%% (target: %d%%)", 
                                    finalMoisture, runningJob.moisture_max);
                    }
            } else if (runningJob.duration > 0) {
                float actualDuration = (now - jobStateTimestamp) / 1000.0f;
                logThrottled("Job finished - Duration: %.1fs (target: %.1fs)", 
                            actualDuration, runningJob.duration);
            } else {
                logThrottled("Job finished successfully");
            }

            // Notify clients of job completion
            notifyClients();
            break;
        }
    }
}