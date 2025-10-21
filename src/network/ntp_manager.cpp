#include "network/ntp_manager.h"
#include "config.h"
#include "utils/logger.h"
#include <time.h>

extern volatile bool otaUpdating;
extern const char* ntpServer1;
extern const char* ntpServer2;
extern const char* ntpServer3;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern const char* timezone;

void handleNTPSync() {
    if (otaUpdating || jobActive) return;
    
    unsigned long now = millis();
    
    switch(ntpCtx.state) {
        case NTP_IDLE:
            if (now - ntpCtx.lastSync >= ntpSyncInterval) {
                ntpCtx.state = NTP_INIT;
                ntpCtx.stateTime = now;
                ntpCtx.syncInProgress = true;
                ntpCtx.retryCount = 0;
                logThrottled("Starting NTP sync...");
                // Don't break - continue to NTP_INIT immediately
            } else {
                break;
            }
            // Fall through to NTP_INIT
            
        case NTP_INIT:
            logThrottled("Configuring NTP...");
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
            ntpCtx.state = NTP_WAITING;
            ntpCtx.stateTime = now;
            break;
            
        case NTP_WAITING: {
            static unsigned long lastNTPWaitLog = 0;
            time_t timeNow = time(nullptr);

            if (now - lastNTPWaitLog >= NTP_WAIT_LOG_INTERVAL) {
                logThrottled("Waiting for valid time... Current: %ld", (long)timeNow);
                lastNTPWaitLog = now;
            }

            // Check if we have a valid time (Unix timestamp after Jan 2, 1970)
            if (timeNow > 86400) {  // More than 1 day since epoch
                struct tm timeinfo;
                localtime_r(&timeNow, &timeinfo);
                setenv("TZ", timezone, 1);
                tzset();
                ntpCtx.state = NTP_DONE;
                ntpCtx.lastSync = now;
                ntpCtx.syncInProgress = false;
                logThrottled("NTP sync complete - Time set to: %04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                // Timeout after 10 seconds
                if (now - ntpCtx.stateTime > ntpMaxWait) {
                    if (++ntpCtx.retryCount >= 3) {
                        ntpCtx.state = NTP_IDLE;
                        ntpCtx.syncInProgress = false;
                        logThrottled("NTP sync failed after 3 retries");
                    } else {
                        ntpCtx.state = NTP_INIT;
                        ntpCtx.stateTime = now;
                        logThrottled("NTP sync retry %d/3", ntpCtx.retryCount);
                    }
                }
            }
            break;
        }
            
        case NTP_DONE:
            ntpCtx.state = NTP_IDLE;
            break;
    }
}