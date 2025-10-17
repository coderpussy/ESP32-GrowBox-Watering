#include "scheduler/job_parser.h"
#include "utils/logger.h"

jobDateTime parseJobDateTime(const char* starttime) {
    jobDateTime dt = {0,0,0,0,0,false,false};
    if (starttime == nullptr || starttime[0] == '\0') return dt;

    int year=0, month=0, day=0, hour=0, minute=0;
    int matched;

    // Try full datetime with both separators
    matched = sscanf(starttime, "%4d-%2d-%2d%*[T ]%2d:%2d", &year, &month, &day, &hour, &minute);
    
    if (matched == 5) {
        logThrottled("Parsed one-time job: %04d-%02d-%02d %02d:%02d", 
            year, month, day, hour, minute);
            
        if (year >= 1970 && month >= 1 && month <= 12 && 
            day >= 1 && day <= 31 && 
            hour >= 0 && hour <= 23 && 
            minute >= 0 && minute <= 59) {
            dt.year = year;
            dt.month = month;
            dt.day = day;
            dt.hour = hour;
            dt.minute = minute;
            dt.valid = true;
            dt.timeOnly = false;
            return dt;
        }
        logThrottled("Invalid date/time ranges in one-time job");
        return dt;
    }

    // Try time-only format
    matched = sscanf(starttime, "%2d:%2d", &hour, &minute);
    if (matched == 2) {
        logThrottled("Parsed everyday job: %02d:%02d", hour, minute);
            
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
            dt.hour = hour;
            dt.minute = minute;
            dt.valid = true;
            dt.timeOnly = true;
            return dt;
        }
        logThrottled("Invalid time ranges in everyday job");
        return dt;
    }

    logThrottled("Failed to parse datetime: %s", starttime);
    return dt;
}