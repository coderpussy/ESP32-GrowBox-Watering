#include "network/wifi_manager.h"
#include "config.h"
#include <WiFi.h>

extern const char* ssid_ap;
extern const char* password_ap;
extern const char* ssid_sta;
extern const char* password_sta;
extern const int WiFiMode_AP_STA;

void initWiFi() {
    int wifi_retry = 0;

    if (WiFiMode_AP_STA == 0) {
        WiFi.mode(WIFI_AP);
        delay(100);
        WiFi.softAP(ssid_ap, password_ap);
        Serial.println("Start WLAN AP");
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Start WLAN Client DHCP");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid_sta, password_sta);

        while (WiFi.status() != WL_CONNECTED) {
            wifi_retry++;
            Serial.print(".");
            delay(1000);
            if (wifi_retry > 10) {
                Serial.println("\nReboot");
                ESP.restart();
            }
        }

        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid_sta);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
}