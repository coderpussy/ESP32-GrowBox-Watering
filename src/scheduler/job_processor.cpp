#include "scheduler/job_processor.h"
#include "scheduler/job_parser.h"
#include "scheduler/job_state_machine.h"
#include "config.h"
#include "utils/logger.h"
#include <time.h>

extern volatile bool otaUpdating;

void jobsProcessor() {
    static int lastCheckedSecond = -1;
    static int lastExecutedJobId = -1;
    static unsigned long lastJobStartTime = 0;
    const int startWindowSec = 30;

    time_t now_t = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now_t, &timeinfo);

    int currentSecond = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    if (currentSecond == lastCheckedSecond) return;
    lastCheckedSecond = currentSecond;

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

    for (const jobStruct& job : joblistVec) {
        if (job.id == lastExecutedJobId) continue;
        if (!job.active || job.starttime[0] == '\0') continue;

        jobDateTime dt = parseJobDateTime(job.starttime);
        if (!dt.valid) continue;

        bool shouldStart = false;
        if (job.everyday || dt.timeOnly) {
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

        if (shouldStart) {
            logThrottled("Job %d scheduled for %02d:%02d should start now", 
                job.id, dt.hour, dt.minute);

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