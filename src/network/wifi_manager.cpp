#include "network/wifi_manager.h"
#include "utils/logger.h"

Preferences preferences;

// WiFi credentials storage keys
const char* PREF_NAMESPACE = "wifi_creds";
const char* PREF_SSID = "ssid";
const char* PREF_PASSWORD = "password";

// Access Point credentials
const char* AP_SSID = "ESP32-WIFI-MANAGER";
const char* AP_PASSWORD = "123456789";

String getWiFiSSID() {
    preferences.begin(PREF_NAMESPACE, true);
    String ssid = preferences.getString(PREF_SSID, "");
    preferences.end();
    return ssid;
}

String getWiFiPassword() {
    preferences.begin(PREF_NAMESPACE, true);
    String password = preferences.getString(PREF_PASSWORD, "");
    preferences.end();
    return password;
}

void saveWiFiCredentials(String ssid, String password) {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putString(PREF_SSID, ssid);
    preferences.putString(PREF_PASSWORD, password);
    preferences.end();
    logThrottled("WiFi credentials saved");
}

void resetWiFiSettings() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    logThrottled("WiFi settings reset");
}

bool initWiFiStation() {
    String ssid = getWiFiSSID();
    String password = getWiFiPassword();

    if (ssid == "" || password == "") {
        logThrottled("No WiFi credentials stored");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    logThrottled("Connecting to WiFi: %s", ssid.c_str());

    unsigned long startAttemptTime = millis();

    // Try to connect for 10 seconds
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        logThrottled("Failed to connect to WiFi");
        return false;
    }

    logThrottled("WiFi Connected!");
    logThrottled("SSID: %s", WiFi.SSID().c_str());
    logThrottled("IP Address: %s", WiFi.localIP().toString().c_str());
    logThrottled("Signal Strength: %d dBm", WiFi.RSSI());
    
    return true;
}

void initWiFiAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    logThrottled("AP Started");
    logThrottled("AP SSID: %s", AP_SSID);
    logThrottled("AP Password: %s", AP_PASSWORD);
    logThrottled("AP IP Address: %s", IP.toString().c_str());
}

void initWiFi() {
    // Try to connect to stored WiFi credentials
    if (!initWiFiStation()) {
        // If connection fails, start Access Point
        initWiFiAP();
    }
}

bool checkWiFiConnection() {
    // Check if we're in station mode and supposed to be connected
    if (WiFi.getMode() != WIFI_STA && WiFi.getMode() != WIFI_AP_STA) {
        return true; // Not in station mode, consider it "ok"
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Check signal strength
        int rssi = WiFi.RSSI();
        if (rssi < -90) {
            logThrottled("Warning: Weak WiFi signal: %d dBm", rssi);
        }
        return true;
    }

    // Connection lost, try to reconnect
    logThrottled("WiFi connection lost. Attempting to reconnect...");
    
    String ssid = getWiFiSSID();
    String password = getWiFiPassword();

    if (ssid == "" || password == "") {
        logThrottled("No stored credentials for reconnection");
        return false;
    }

    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();

    // Try to reconnect for 15 seconds
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        logThrottled("WiFi reconnected successfully");
        logThrottled("IP Address: %s", WiFi.localIP().toString().c_str());
        logThrottled("Signal Strength: %d dBm", WiFi.RSSI());
        return true;
    }

    logThrottled("WiFi reconnection failed");
    return false;
}