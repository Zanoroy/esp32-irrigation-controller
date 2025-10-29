# ESP32 Hunter X-Core Irrigation Controller

A comprehensive WiFi-enabled irrigation controller for Hunter X-Core sprinkler systems, built on ESP32 with REST API, timezone support, and advanced zone management.

## 🚿 Features

### Core Functionality
- **16-Zone Support**: Hardware support for up to 16 zones (configurable active zones)
- **WiFi Control**: Complete REST API for remote irrigation management
- **Real-Time Clock**: DS1307 RTC with NTP synchronization
- **Timezone Support**: Half-hour precision timezone handling (e.g., Adelaide UTC+9:30)
- **Zone Validation**: Configurable maximum enabled zones with access control
- **Pump Safety**: Automatic pump management with safety shutoffs

### Advanced Features
- **Configuration Management**: Persistent settings stored in ESP32 NVS
- **JSON API**: All endpoints return structured JSON responses
- **Zone Timers**: Individual zone runtime management with safety limits
- **NTP Synchronization**: Automatic time sync with configurable servers
- **Web Interface**: Built-in web interface for manual control

## 🔧 Hardware Requirements

### Components
- **ESP32-WROOM-32** (ESP32 DevKit V1)
- **Hunter X-Core** irrigation controller
- **TinyRTC DS1307** real-time clock module
- **Relay Module** (for pump control)
- **Level Shifters** (3.3V to 5V for Hunter interface)

### Pin Configuration
```cpp
GPIO16  - Hunter X-Core data line
GPIO5   - Pump relay control
GPIO21  - I2C SDA (RTC)
GPIO22  - I2C SCL (RTC)
```

## 📡 API Documentation

All endpoints return JSON responses with `status`, `message`, and relevant data fields.

### Configuration
```bash
GET  /api/config                    # Get system configuration
POST /api/config?param=value        # Update configuration
```

### Zone Control
```bash
GET /api/start-zone?zone=1&time=10   # Start zone for specified minutes
GET /api/stop-zone?zone=1            # Stop specific zone
```

### System Status
```bash
GET /api/status                      # System status and diagnostics
GET /api/time                        # Current local time
GET /api/sync-ntp                    # Manual NTP synchronization
```

### Configuration Parameters
- `timezone`: Timezone offset (e.g., 9.5 for Adelaide)
- `daylight_saving`: Enable/disable DST (true/false)
- `max_enabled_zones`: Maximum active zones (1-16)
- `max_runtime`: Maximum zone runtime in minutes
- `auto_ntp`: Enable automatic NTP sync
- `pump_safety`: Enable pump safety mode

## 🌏 Timezone Support

The system supports half-hour timezone increments for global compatibility:

```bash
# Australia/Adelaide (UTC+9:30)
curl -X POST "http://DEVICE_IP/api/config?timezone=9.5&daylight_saving=true"

# US/Eastern (UTC-5:00)
curl -X POST "http://DEVICE_IP/api/config?timezone=-5&daylight_saving=true"
```

## 🚀 Quick Start

### 1. Hardware Setup
- Connect ESP32 to Hunter X-Core data line (GPIO16)
- Wire TinyRTC module to I2C pins (21/22)
- Connect pump relay to GPIO5

### 2. Software Installation
```bash
# Clone repository
git clone https://github.com/zanoroy/esp32-irrigation-controller.git
cd esp32-irrigation-controller

# Install PlatformIO
pip install platformio

# Build and upload
pio run --target upload --environment esp32doit-devkit-v1
```

### 3. WiFi Configuration
Update WiFi credentials in `src/main.cpp`:
```cpp
const char* ssid = "Your_WiFi_SSID";
const char* password = "Your_WiFi_Password";
```

### 4. Initial Configuration
```bash
# Set your timezone (example: Adelaide, Australia)
curl -X POST "http://DEVICE_IP/api/config?timezone=9.5&daylight_saving=true&max_enabled_zones=8"
```

## 📋 Example Usage

### Start Zone 1 for 15 minutes
```bash
curl "http://192.168.1.100/api/start-zone?zone=1&time=15"
```
Response:
```json
{
  "status": "success",
  "message": "Zone 1 started for 15 minutes",
  "zone": 1,
  "duration_minutes": 15
}
```

### Check System Status
```bash
curl "http://192.168.1.100/api/status"
```
Response:
```json
{
  "status": "success",
  "system": {
    "free_heap": 240952,
    "uptime_seconds": 3600,
    "wifi_rssi": -35,
    "ip_address": "192.168.1.100"
  },
  "pump": {"status": "OFF"},
  "rtc": {
    "status": "connected",
    "local_time": "2025-10-30 09:33:45 (UTC+9:30 DST)"
  }
}
```

## 🛠️ Development

### Project Structure
```
hunter_wifi_remote/
├── src/
│   ├── main.cpp              # Main application entry
│   ├── web_server.cpp        # REST API server
│   ├── config_manager.cpp    # Configuration management
│   ├── rtc_module.cpp        # RTC and NTP functions
│   └── hunter_esp32.cpp      # Hunter X-Core interface
├── include/
│   ├── web_server.h
│   ├── config_manager.h
│   ├── rtc_module.h
│   └── hunter_esp32.h
└── platformio.ini            # PlatformIO configuration
```

### Key Classes
- **ConfigManager**: Persistent configuration with NVS storage
- **HunterWebServer**: REST API endpoints and zone control
- **RTCModule**: Time management and NTP synchronization
- **HunterESP32**: Hunter X-Core protocol implementation

## 🔒 Zone Management

The system supports configurable zone limits for safety and control:

```bash
# Enable only 8 zones (zones 9-16 will be disabled)
curl -X POST "http://DEVICE_IP/api/config?max_enabled_zones=8"

# Attempting to use zone 9 returns error:
curl "http://DEVICE_IP/api/start-zone?zone=9&time=10"
# Response: {"status":"error","message":"Zone 9 is not enabled. Maximum enabled zones: 8"}
```

## 📝 License

This project is open source. Feel free to modify and distribute according to your needs.

## 🤝 Contributing

Contributions welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## 📞 Support

For support and questions, please open an issue on GitHub.

---

**Built with ❤️ for the irrigation community**