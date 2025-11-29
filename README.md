# ESP32 Growbox Watering System

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-blue.svg)](https://platformio.org/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.9.1-orange.svg)](CHANGELOG.md)

An intelligent automated watering system for growboxes, built on ESP32 with support for multiple plants, moisture sensors, flow sensors, and flexible scheduling.

**Content is AI generated and contains errors.**

**Installation and configuration prerequisits may differ from authors development usage with PlatformIO VS-Code integration. This section will be updated in the future.**

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Software Architecture](#-software-architecture)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage](#-usage)
- [Web Interface](#-web-interface)
- [API Documentation](#-api-documentation)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)
- [License](#-license)
- [Credits](#-credits)

## ✨ Features

### Core Functionality
- **Multi-Plant Support** - Control up to 8 independent watering zones
- **Smart Scheduling** - Time-based, moisture-based, or hybrid watering schedules
- **Flow Monitoring** - Track water consumption per job with YF-S201 flow sensor
- **Moisture Sensing** - Real-time soil moisture monitoring for each plant
- **Web Interface** - Responsive web UI with dark mode support
- **OTA Updates** - Over-the-air firmware updates via ArduinoOTA
- **Multi-Language** - Support for English and German (easily extendable)

### Watering Control Modes
1. **Time-Based** - Water for a specific duration (seconds)
2. **Volume-Based** - Water until a target volume is reached (milliliters)
3. **Moisture-Based** - Water until soil moisture reaches target level (%)

### Trigger Types
1. **Scheduled** - Water at specific times (daily/one-time)
2. **Moisture-Triggered** - Water when soil gets too dry
3. **Hybrid** - Combine both triggers (OR logic)

### Safety Features
- 10-minute maximum job timeout
- Moisture sensor stuck detection
- Emergency stop functionality
- State machine-based control for reliable operation
- Interrupt-safe flow sensor pulse counting

## 🔧 Hardware Requirements

### Required Components
- **ESP32 Development Board** (e.g., ESP32 DevKit v1, DOIT ESP32)
- **ESP32 Development Motherboard** (used to expose pin management)
- **12V Water Pump** (submersible recommended)
- **12V Solenoid Valves** (one per plant zone)
- **12V to 5V Buck Converter** (for powering ESP32)
- **12V Relay Module** (for valve and pump control, channels = number of plants + 1 for pump)
- **12V Power Supply** (minimum 2A)

### Optional Components
- **YF-S201 Flow Sensor** (for volume-based watering)
- **Capacitive Soil Moisture Sensors** (one per plant)
- **Level Shifter** (if using 3.3V sensors with 5V logic)

### Wiring Diagram

```
ESP32 GPIO Pins:
├─ GPIO 13  → Pump Relay (IN)
├─ GPIO 14  → Flow Sensor (Signal)
├─ GPIO 25+ → Valve Relays (configurable, sequential)
└─ GPIO 32+ → Moisture Sensors (analog, sequential)

Power:
├─ ESP32 5V  ← USB or Buck Converter
├─ Pump 12V  ← Power Supply (through relay)
└─ Valves 12V ← Power Supply (through relays)
```

## 🏗️ Software Architecture

### Project Structure
```
ESP32-Growbox-Watering/
├── data/                     # Web interface files
│   ├── favicon.ico           # Favicon
│   ├── index.html            # Main web page
│   ├── wifimanager.html      # WiFi Manager web page
│   ├── css/                  # Stylesheets
│   │   ├── calendar.css
│   │   ├── darkmode.css
│   │   ├── gear.css
│   │   ├── index.css
│   │   ├── moisturesensor.css
│   │   ├── plug.css
│   │   ├── scheduler.css
│   │   ├── wifi.css
│   │   └── wifimanager.css
│   ├── js/                   # JavaScript modules
│   │   ├── clock.js          # Clock logic
│   │   ├── hardware.js       # Hardware update logic
│   │   ├── index.js          # Main application logic
│   │   ├── language.js       # Multi-language support
│   │   ├── scheduler.js      # Jobs configuration
│   │   └── wifimanager.js    # WiFi configuration
│   └── lang/                 # Language files
│       ├── en.json
│       └── de.json
├── include/                  # Header files
│   ├── config.h              # Global configuration
│   ├── hardware/
│   │   ├── pin_manager.h
│   │   ├── valve_control.h
│   │   ├── pump_control.h
│   │   ├── moisture_sensor.h
│   │   └── flow_sensor.h
│   ├── network/
│   │   ├── wifi_manager.h
│   │   ├── websocket_handler.h
│   │   └── ntp_manager.h
│   ├── scheduler/
│   │   ├── job_processor.h
│   │   ├── job_state_machine.h
│   │   └── job_parser.h
│   ├── storage/
│   │   ├── filesystem_manager.h
│   │   └── config_manager.h
│   └── utils/
│       └── logger.h
├── src/                      # Source files
│   ├── main.cpp              # Entry point
│   ├── hardware/             # Hardware control modules
│   │   ├── pin_manager.cpp
│   │   ├── valve_control.cpp
│   │   ├── pump_control.cpp
│   │   ├── moisture_sensor.cpp
│   │   └── flow_sensor.cpp
│   ├── network/              # Network management
│   │   ├── wifi_manager.cpp
│   │   ├── websocket_handler.cpp
│   │   └── ntp_manager.cpp
│   ├── scheduler/            # Job scheduling logic
│   │   ├── job_processor.cpp
│   │   ├── job_state_machine.cpp
│   │   └── job_parser.cpp
│   ├── storage/              # Configuration & persistence
│   │   ├── filesystem_manager.cpp
│   │   └── config_manager.cpp
│   └── utils/                # Utility functions
│       └── logger.cpp
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

### State Machines

**Job State Machine:**
```
IDLE → VALVE_OPENING → PUMP_STARTING → RUNNING → 
PUMP_STOPPING → VALVE_CLOSING → IDLE
```

**Pump State Machine:**
```
IDLE ⇄ STARTING → RUNNING → STOPPING → IDLE
```

## 📦 Installation

### Prerequisites
- [PlatformIO IDE](https://platformio.org/install/ide) or [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html)
- USB cable for ESP32 programming
- WiFi network (2.4GHz)

### Step 1: Clone Repository
```bash
git clone https://github.com/coderpussy/ESP32-Growbox-Watering.git
cd ESP32-Growbox-Watering
```

### Step 2: Install Dependencies
```bash
pio pkg install
```

Dependencies (automatically installed):
- ESP32 Arduino Framework
- ESPAsyncWebServer
- AsyncTCP
- ArduinoJson
- LittleFS
- WebSerialLite

### Step 3: Configure WiFi (Optional)
Edit `data/settings.json` with your WiFi credentials:
```json
{
  "ssid": "YourWiFiSSID",
  "password": "YourWiFiPassword",
  "hostname": "growbox-watering"
}
```

Or use the web interface's WiFi Manager after first boot (AP mode).

### Step 4: Upload Filesystem
```bash
pio run --target uploadfs
```

### Step 5: Compile and Upload Firmware
```bash
pio run --target upload
```

### Step 6: Monitor Serial Output
```bash
pio device monitor
```

## ⚙️ Configuration

### System Settings (`data/config.json`)
```json
{
  "use_webserial": false,
  "use_flowsensor": true,
  "use_moisturesensor": true,
  "auto_switch": true,
  "plant_count": 3,
  "valve_start_pin": 25
}
```

**Parameters:**
- `use_webserial` - Enable browser-based serial monitor
- `use_flowsensor` - Enable flow sensor readings
- `use_moisturesensor` - Enable moisture sensor readings
- `auto_switch` - Enable automatic job scheduling
- `plant_count` - Number of watering zones (1-8)
- `valve_start_pin` - Starting GPIO pin for valves (sequential)

### Job Scheduling (`data/schedules.json`)
```json
{
  "jobs": [
    {
      "id": 1,
      "valve": 0,
      "active": true,
      "duration": 30.0,
      "volume": 0.0,
      "moisture_min": 0,
      "moisture_max": 0,
      "starttime": "08:00",
      "everyday": true,
      "trigger_type": 0,
      "moisture_threshold": 0
    }
  ]
}
```

**Job Parameters:**
- `id` - Unique job identifier
- `valve` - Plant zone (0-based index)
- `active` - Enable/disable job
- `duration` - Watering duration in seconds (0 = disabled)
- `volume` - Target volume in milliliters (0 = disabled)
- `moisture_min` - Start watering at this % (for moisture-based)
- `moisture_max` - Stop watering at this % (for moisture-based)
- `starttime` - Time in "HH:MM" format
- `everyday` - Repeat daily (true) or one-time (false)
- `trigger_type` - 0=Time, 1=Moisture, 2=Both
- `moisture_threshold` - Trigger threshold for moisture-based jobs

### Sensor Calibration

**Moisture Sensors** (`src/hardware/moisture_sensor.cpp`):
```cpp
const int DRY_ANALOG_VALUE = 4095;  // Dry soil reading
const int WET_ANALOG_VALUE = 1500;  // Wet soil reading
```

**Flow Sensor** (`src/hardware/flow_sensor.cpp`):
```cpp
const float FLOW_CALIBRATION_FACTOR = 7.5;  // YF-S201: 7.5 pulses/sec per L/min
```

## 🖥️ Usage

### Initial Setup

1. **Power on** the ESP32
2. **Connect to AP** - Look for `ESP32-Watering-Setup` WiFi network
3. **Open browser** - Navigate to `http://192.168.4.1`
4. **Configure WiFi** - Use WiFi Manager to connect to your network
5. **Access web interface** - Find ESP32 IP in your router or serial monitor

### Web Interface

Access at `http://<ESP32-IP>` or `http://growbox-watering.local`

**Main Features:**
- 🎛️ **Manual Controls** - Test individual valves and pump
- 📊 **Real-time Monitoring** - Flow rate, volume, moisture levels
- 📅 **Job Scheduler** - Create/edit/delete watering schedules
- ⚙️ **Settings** - Configure system parameters
- 🌐 **WiFi Manager** - Change network settings
- 🔄 **OTA Updates** - Upload new firmware via web

### Creating a Watering Job

**Example 1: Daily time-based watering**
```json
{
  "valve": 0,
  "duration": 45.0,
  "starttime": "07:00",
  "everyday": true,
  "trigger_type": 0
}
```
Waters Plant 1 for 45 seconds every day at 7:00 AM

**Example 2: Moisture-triggered watering**
```json
{
  "valve": 1,
  "moisture_min": 15,
  "moisture_max": 70,
  "trigger_type": 1,
  "moisture_threshold": 20
}
```
Waters Plant 2 when moisture drops below 20%, stops at 70%

**Example 3: Volume-based watering**
```json
{
  "valve": 2,
  "volume": 500.0,
  "starttime": "18:00",
  "everyday": true,
  "trigger_type": 0
}
```
Waters Plant 3 with exactly 500ml every day at 6:00 PM

### Manual Operation

Use the web interface controls:
1. **Pump Switch** - Toggle pump on/off
2. **Valve Buttons** - Open/close individual valves
3. **Emergency Stop** - Immediately stop all operations

### Monitoring

**Real-time Data:**
- Current flow rate (ml/min)
- Total volume dispensed (ml)
- Pump runtime
- Moisture levels per plant (%)
- Active job status

**Logs:**
- Serial monitor (`pio device monitor`)
- WebSerial (if enabled in settings)
- Browser console (WebSocket messages)

## 📡 API Documentation

### WebSocket API

**Connection:** `ws://<ESP32-IP>/ws`

**Client → Server:**
```javascript
// Get system status
ws.send(JSON.stringify({ action: "getStatus" }));

// Toggle pump
ws.send(JSON.stringify({ action: "togglePump" }));

// Toggle valve
ws.send(JSON.stringify({ action: "toggleValve", valve: 0 }));

// Save job
ws.send(JSON.stringify({
  action: "saveJob",
  job: { /* job object */ }
}));
```

**Server → Client:**
```javascript
// Status update
{
  "type": "status",
  "pump": true,
  "valves": [false, true, false],
  "flowRate": 1250.5,
  "flowVolume": 2500.0,
  "moisture": [45, 62, 38],
  "jobActive": true,
  "autoSwitch": true
}

// Log message
{
  "type": "log",
  "message": "Job 1 started for valve 2"
}
```

### REST API Endpoints

```
GET  /                    - Serve web interface
GET  /scan                - Scan WiFi networks
GET  /scan-results        - Get scan results (JSON)
POST /connect             - Connect to WiFi
GET  /reset-wifi          - Reset WiFi to AP mode
GET  /config.json         - Get system configuration
POST /config.json         - Update system configuration
GET  /schedules.json      - Get job schedules
POST /schedules.json      - Update job schedules
```

## 🐛 Troubleshooting

### ESP32 won't connect to WiFi
1. Check 2.4GHz band (ESP32 doesn't support 5GHz)
2. Verify credentials in `data/settings.json`
3. Reset WiFi via web interface or serial command
4. Check signal strength (move closer to router)

### Flow sensor not reading
1. Verify wiring (GPIO 14, VCC, GND)
2. Check `use_flowsensor: true` in config
3. Test with manual pump activation
4. Inspect sensor for blockage or debris

### Moisture sensors show incorrect values
1. Calibrate sensors (adjust `DRY_ANALOG_VALUE` and `WET_ANALOG_VALUE`)
2. Check pin assignments (starting from GPIO 32)
3. Ensure sensors are capacitive (resistive sensors degrade quickly)
4. Verify 3.3V power (ESP32 ADC limitation)

### Jobs not executing
1. Enable `auto_switch: true` in config
2. Check `job.active: true` in schedules
3. Verify NTP sync (jobs need correct time)
4. Check serial logs for errors
5. Ensure job trigger conditions are met

### Pump doesn't start
1. Verify relay wiring (GPIO 13)
2. Check 12V power supply
3. Test relay manually (toggle in web interface)
4. Inspect pump for mechanical issues

### Web interface not loading
1. Check ESP32 IP address (serial monitor or router)
2. Verify filesystem upload (`pio run --target uploadfs`)
3. Clear browser cache
4. Try different browser or incognito mode

### OTA update fails
1. Ensure stable WiFi connection
2. Use firmware file (`.bin`) not source code
3. Check available flash space
4. Wait for current job to complete

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

### Reporting Issues
1. Check existing issues first
2. Include ESP32 model and firmware version
3. Provide serial logs or screenshots
4. Describe expected vs. actual behavior

### Submitting Pull Requests
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow code style (see `.clang-format`)
4. Test thoroughly on hardware
5. Update documentation
6. Commit with clear messages (`git commit -m 'Add amazing feature'`)
7. Push to branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### Code Style
- Use 4 spaces for indentation
- Comment complex logic
- Follow existing naming conventions
- Keep functions focused and small

### Testing Checklist
- [ ] Compiles without warnings
- [ ] Filesystem uploads successfully
- [ ] Web interface loads correctly
- [ ] Manual controls work
- [ ] Jobs execute as expected
- [ ] No memory leaks (monitor heap)
- [ ] OTA update succeeds

## 📝 ToDo

- [ ] Add MQTT support for Home Assistant integration
- [ ] Implement water tank level sensor
- [ ] Add fertilizer dosing control
- [ ] Create mobile app (React Native)
- [ ] Support for pH/EC sensors
- [ ] Data logging and analytics (InfluxDB/Grafana)
- [ ] Multi-pump support for larger setups
- [ ] Rain delay feature (weather API integration)
- [ ] Push notifications (email/Telegram)
- [ ] Backup/restore configuration

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Credits

### Author
- **Maja Aurora** - Initial work - [GitHub Profile](https://github.com/coderpussy)

### Libraries
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Async web server
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) - Async TCP library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [WebSerialLite](https://github.com/ayushsharma82/WebSerial) - Browser-based serial monitor

### Inspiration
- ESP32 community forums and examples
- Home automation enthusiasts
- Hydroponic/aquaponic community
- Plant watering during vacation

### Contributors
See [CONTRIBUTORS.md](CONTRIBUTORS.md) for a list of contributors.

---

## 📞 Support

- **Documentation:** [Wiki](https://github.com/coderpussy/ESP32-Growbox-Watering/wiki)
- **Issues:** [GitHub Issues](https://github.com/coderpussy/ESP32-Growbox-Watering/issues)
- **Discussions:** [GitHub Discussions](https://github.com/coderpussy/ESP32-Growbox-Watering/discussions)

---

**Made with ❤️ for plants and automation enthusiasts**

⭐ Star this repository if you find it helpful!