// Version 0.9, 22.06.2025, coderpussy
const char* APP_VERSION = "0.9";

// Correction for compass direction of pcb mount
//#define Az_PCB_Correction 90

// If need to format LITTLEFS filesystem due to corruption
// After formatting LITTLEFS, please set below variable to false,
// recompile and upload sketch to store files permanently
#define FORMAT_LITTLEFS_IF_FAILED false

// Wifi: Select AP or Client
#define WiFiMode_AP_STA 1                               // Defines WiFi Mode 0 -> AP (with IP:192.168.4.1 and  1 -> Station (client with IP: via DHCP)

// Enter your AP SSID and PASSWORD
const char* ssid_ap = "growboxwatering";                // Set AP SSID name
const char* password_ap = "boxxxxx";                    // Set AP password

// Enter your STA SSID and PASSWORD
const char* ssid_sta = "XYZ-WLAN";              // Set STA SSID name
const char* password_sta = "xxxxxxxxxxxxxxxxx";      // Set STA password

// Timezone string for your region, example: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // CET-1CEST,M3.5.0,M10.5.0/3 = Europe/Berlin

// Astra 19.2 position and dish specific offsets
//float Astra_Az = 173.34,
//      Astra_El = 29.40,
//      El_Offset = -18.00,
//      Az_Offset = -10.00;

// Actuator speed
//int motorSpeed = 700;                                   // Change (lower) this if dish is begining to swing

// Define config file on LittleFS
const char* configfile = "/settings.json";

// Define jobs file on LittleFS
const char* jobsfile = "/schedules.json";
