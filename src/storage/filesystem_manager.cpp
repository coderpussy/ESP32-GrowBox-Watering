#include "storage/filesystem_manager.h"
#include "config.h"
#include "utils/logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define FORMAT_LITTLEFS_IF_FAILED false

void initFS() {
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");
}

void printFile(const char* filename) {
    File file = LittleFS.open(filename);
    if (!file) {
        logThrottled("Failed to read file");
        return;
    }

    String content = file.readString();
    file.close();

    if (content.length() == 0) {
        logThrottled("File is empty");
        return;
    }

    logThrottled("%s", content.c_str());
}

void loadJobList(const char* jobsfile) {
    if (!LittleFS.exists(jobsfile)) {
        logThrottled("Jobs file '%s' does not exist", jobsfile);
        return;
    }

    File file = LittleFS.open(jobsfile, "r");
    if (!file) {
        logThrottled("Failed to open jobs file: %s", jobsfile);
        return;
    }

    String content = file.readString();
    file.close();

    if (content.length() == 0) {
        logThrottled("Jobs file is empty");
        return;
    }

    size_t cap = (size_t)content.length() * 2 + 1024;
    const size_t CAP_MAX = 64 * 1024;
    if (cap > CAP_MAX) cap = CAP_MAX;

    DynamicJsonDocument doc(cap);
    DeserializationError err = deserializeJson(doc, content);

    if (err) {
        logThrottled("Invalid JSON in jobs file: %s", err.c_str());
        return;
    }

    if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        joblistVec.clear();

        for (JsonObject obj : arr) {
            jobStruct job;
            job.id = obj["id"] | 0;
            job.active = obj["active"] | false;
            strlcpy(job.name, obj["name"] | "", sizeof(job.name));
            strlcpy(job.job, obj["job"] | "", sizeof(job.job));
            strlcpy(job.plant, obj["plant"] | "", sizeof(job.plant));
            job.duration = obj["duration"] | 0;
            strlcpy(job.starttime, obj["starttime"] | "", sizeof(job.starttime));
            job.everyday = obj["everyday"] | false;
            joblistVec.push_back(job);
        }

        logThrottled("Loaded %d job(s)", joblistVec.size());
    }
}

void saveJobList(const char* jobsfile) {
    if (LittleFS.exists(jobsfile)) {
        if (!LittleFS.remove(jobsfile)) {
            logThrottled("Failed to remove existing jobs file");
            return;
        }
    }

    File file = LittleFS.open(jobsfile, "w");
    if (!file) {
        logThrottled("Failed to create job schedules file");
        return;
    }

    int joblen = joblistVec.size();
    if (joblen == 0) {
        logThrottled("No jobs to save");
        file.close();
        return;
    }

    const size_t capacity = JSON_ARRAY_SIZE(joblen) + 
                          (joblen * JSON_OBJECT_SIZE(8)) +
                          (joblen * 128);
    
    DynamicJsonDocument doc(capacity);
    JsonArray joblistArray = doc.to<JsonArray>();

    for (size_t i = 0; i < joblen; i++) {
        const jobStruct& job = joblistVec[i];
        JsonObject obj = joblistArray.createNestedObject();
        
        obj["id"] = job.id;
        obj["active"] = job.active;
        obj["name"] = job.name;
        obj["job"] = job.job;
        obj["plant"] = job.plant;
        obj["duration"] = job.duration;
        obj["starttime"] = job.starttime;
        obj["everyday"] = job.everyday;
    }

    size_t bytesWritten = serializeJson(doc, file);
    file.close();

    if (bytesWritten == 0) {
        logThrottled("Failed to write jobs to file");
        return;
    }

    logThrottled("Successfully saved %d jobs (%d bytes)", joblen, bytesWritten);
}

void deleteJobList(const char* jobsfile) {
    if (!LittleFS.exists(jobsfile)) {
        logThrottled("Jobs file does not exist");
        return;
    }

    if (!LittleFS.remove(jobsfile)) {
        logThrottled("Failed to delete jobs file");
        return;
    }

    logThrottled("Deleted jobs file");
    joblistVec.clear();
}