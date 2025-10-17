#include "utils/logger.h"
#include "config.h"
#include <WebSerialLite.h>
#include <queue>

static unsigned long lastLogMillis = 0;
static std::queue<String> webSerialQueue;

void initLogger() {
    Serial.begin(115200);
    lastLogMillis = 0;
}

void logThrottled(const char* format, ...) {
    unsigned long now = millis();
    if (now - lastLogMillis < LOG_THROTTLE_MS) return;
    lastLogMillis = now;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.println(buffer);

    if (settings.use_webserial) {
        queueWebSerial(buffer);
    }
}

void queueWebSerial(const char* message) {
    if (webSerialQueue.size() < 50) {
        webSerialQueue.push(String(message));
    }
}

void processWebSerialQueue() {
    static unsigned long lastFlush = 0;
    unsigned long now = millis();

    if (now - lastFlush >= WEBSERIAL_FLUSH_INTERVAL && !webSerialQueue.empty()) {
        String msg = webSerialQueue.front();
        webSerialQueue.pop();
        WebSerial.println(msg);
        lastFlush = now;
    }
}