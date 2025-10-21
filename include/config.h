#pragma once

#include <Arduino.h>
#include <vector>

// Version
extern const char* APP_VERSION;

// Pin Definitions
const int pumpPin = 33;
const int soilFlowSensorPin = 19;

// Timing Constants
static const unsigned long LOG_THROTTLE_MS = 0; //100;
static const unsigned long WEBSERIAL_FLUSH_INTERVAL = 0; //50;
static const unsigned long JOB_CHECK_INTERVAL = 1000;
static const unsigned long NTP_WAIT_LOG_INTERVAL = 2000;
static const unsigned long WIFI_CHECK_INTERVAL = 30000;  // Check WiFi every 30 seconds

// NTP Configuration
extern const char* ntpServer1;
extern const char* ntpServer2;
extern const char* ntpServer3;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern const char* timezone;
const long ntpSyncInterval = 3600000; // 1 hour in milliseconds 60 * 60 * 1000
const unsigned long ntpMaxWait = 10000;

// Settings Structure
struct Settings {
    bool use_webserial = false;
    bool use_flowsensor = false;
    bool use_moisturesensor = false;
    bool auto_switch = false;
    uint8_t plant_count = 3;
    int valve_start_pin = 25;
};

// Job Structure
struct jobStruct {
    int id;
    bool active;
    char name[32];
    char job[16];
    char plant[16];
    int duration;
    char starttime[20];
    bool everyday;
};

// Job DateTime Structure
struct jobDateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    bool valid;
    bool timeOnly;
};

// State Enums
enum JobState {
    JOB_IDLE,
    JOB_OPEN_VALVE,
    JOB_START_PUMP,
    JOB_RUNNING,
    JOB_STOP_PUMP,
    JOB_CLOSE_VALVE
};

enum PumpState { 
    PUMP_IDLE,
    PUMP_STARTING,
    PUMP_RUNNING,
    PUMP_STOPPING
};

enum NtpState {
    NTP_IDLE,
    NTP_INIT,
    NTP_WAITING,
    NTP_DONE
};

// Pump Context
struct PumpContext {
    PumpState state;
    unsigned long stateTime;
    bool manualControl;
    bool targetState;
};

// NTP Context
struct NtpContext {
    NtpState state;
    unsigned long stateTime;
    unsigned long lastSync;
    int retryCount;
    bool syncInProgress;
};

// Global State Variables
extern Settings settings;
extern std::vector<jobStruct> joblistVec;
extern std::vector<int> valvePins;
extern std::vector<bool> valve_switches;
extern std::vector<int> valveStates;
extern bool auto_switch;
extern bool pump_switch;
extern int pumpState;
extern float pumpRunTime;
extern unsigned long pumpStartMillis;
extern bool jobActive;
extern JobState currentJobState;
extern unsigned long jobStateTimestamp;
extern jobStruct runningJob;
extern PumpContext pumpCtx;
extern NtpContext ntpCtx;