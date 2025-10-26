#include "scheduler/job_processor.h"
#include "scheduler/job_parser.h"
#include "scheduler/job_state_machine.h"
#include "hardware/moisture_sensor.h"
#include "config.h"
#include "utils/logger.h"
#include <time.h>

extern volatile bool otaUpdating;

// Check if moisture-based job should trigger
bool checkMoistureTrigger(const jobStruct& job) {
    if (!settings.use_moisturesensor) {
        return false;
    }

    // job_valve is 0-based index
    if (job.plant < 0 || job.plant >= moistureSensors.size()) {
        logThrottled("Job %d: Invalid valve/sensor index %d", job.id, job.plant);
        return false;
    }

    const MoistureSensorData& sensor = moistureSensors[job.plant];
    
    // Trigger if current moisture is below threshold
    if (sensor.percentValue <= job.moisture_min) {
        logThrottled("Job %d: Moisture trigger - Plant %d moisture %d%% <= threshold %d%%",
                     job.id, job.plant + 1, sensor.percentValue, job.moisture_min);
        return true;
    }
    // Trigger if current moisture is above max threshold
    if (sensor.percentValue >= job.moisture_max) {
        logThrottled("Job %d: Moisture trigger - Plant %d moisture %d%% >= max %d%%",
                     job.id, job.plant + 1, sensor.percentValue, job.moisture_max);
        return true;
    }
    // Trigger if within min-max range
    if (sensor.percentValue >= job.moisture_min && sensor.percentValue <= job.moisture_max) {
        logThrottled("Job %d: Moisture trigger - Plant %d moisture %d%% within range %d%%-%d%%",
                     job.id, job.plant + 1, sensor.percentValue, job.moisture_min, job.moisture_max);
        return true;
    }

    return false;
}

// Check if time-based job should trigger
bool checkTimeTrigger(const jobStruct& job, int currentSecond, time_t now_t) {
    if (job.starttime[0] == '\0') {
        return false;
    }

    jobDateTime dt = parseJobDateTime(job.starttime);
    if (!dt.valid) {
        return false;
    }

    const int startWindowSec = 30;
    bool shouldStart = false;

    if (job.everyday || dt.timeOnly) {
        // Daily recurring job
        int jobSecondsOfDay = dt.hour * 3600 + dt.minute * 60;
        int diff = currentSecond - jobSecondsOfDay;
        if (diff < 0) diff = -diff;
        if (diff <= startWindowSec) shouldStart = true;
    } else {
        struct tm jobtm = {0};
        jobtm.tm_year = dt.year - 1900;
        jobtm.tm_mon = dt.month - 1;
        jobtm.tm_mday = dt.day;
        jobtm.tm_hour = dt.hour;
        jobtm.tm_min = dt.minute;
        jobtm.tm_sec = 0;
        
        struct tm currenttm;
        localtime_r(&now_t, &currenttm);
        
        if (currenttm.tm_year == (dt.year - 1900) &&
            currenttm.tm_mon == (dt.month - 1) &&
            currenttm.tm_mday == dt.day) {
            
            int jobSecondsOfDay = dt.hour * 3600 + dt.minute * 60;
            int currentSecondsOfDay = currenttm.tm_hour * 3600 + 
                                    currenttm.tm_min * 60 + 
                                    currenttm.tm_sec;
            int diff = currentSecondsOfDay - jobSecondsOfDay;
            if (diff < 0) diff = -diff;
            
            if (diff <= startWindowSec) {
                shouldStart = true;
            }
        }
    }

    return shouldStart;
}

// Process a job based on its trigger type
void jobsProcessor() {
    static int lastCheckedSecond = -1;
    static int lastExecutedJobId = -1;
    static unsigned long lastJobStartTime = 0;
    static unsigned long lastMoistureCheck = 0;
    const unsigned long moistureCheckInterval = 300000; // Check moisture jobs every 5 minutes

    time_t now_t = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now_t, &timeinfo);

    int currentSecond = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    bool newSecond = (currentSecond != lastCheckedSecond);
    if (newSecond) {
        lastCheckedSecond = currentSecond;
    }

    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        lastExecutedJobId = -1;
    }

    if (otaUpdating) {
        logThrottled("OTA in progress - skipping job evaluation");
        return;
    }

    unsigned long now = millis();

    if (now - lastJobStartTime < 60000) {
        return;
    }

    bool checkMoisture = (now - lastMoistureCheck >= moistureCheckInterval);
    if (checkMoisture) {
        lastMoistureCheck = now;
    }

    for (const jobStruct& job : joblistVec) {
        // Skip if this job was the last executed one
        if (job.id == lastExecutedJobId) continue;
        // Skip if job is not active
        if (!job.active) continue;

        bool shouldTrigger = false;
        const char* triggerReason = "";

        switch (job.type) {
            case TRIGGER_TIME:
                // Only check time-based triggers on new seconds
                if (newSecond && checkTimeTrigger(job, currentSecond, now_t)) {
                    shouldTrigger = true;
                    triggerReason = "time-based";
                }
                break;

            case TRIGGER_MOISTURE:
                // Check moisture triggers periodically
                if (checkMoisture && checkMoistureTrigger(job)) {
                    shouldTrigger = true;
                    triggerReason = "moisture-based";
                }
                break;

            case TRIGGER_BOTH:
                // Check both conditions
                if (newSecond && checkTimeTrigger(job, currentSecond, now_t)) {
                    shouldTrigger = true;
                    triggerReason = "time-based";
                } else if (checkMoisture && checkMoistureTrigger(job)) {
                    shouldTrigger = true;
                    triggerReason = "moisture-based";
                }
                break;
        }

        if (shouldTrigger) {
            logThrottled("Job %d triggered (%s) - valve %d, duration %.1fs",
                         job.id, triggerReason, job.plant + 1, job.duration);

            if (jobActive) {
                logThrottled("Job %d due but another job active - skipping", job.id);
                continue;
            }

            processJob(job);
            lastExecutedJobId = job.id;
            lastJobStartTime = now;
            break;
        }
    }
}