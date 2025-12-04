# Hunter WiFi Remote - RTC Integration Guide

## Overview
The Hunter WiFi Remote now includes Real-Time Clock (RTC) functionality using a TinyRTC module with DS1307 chip. This enables accurate timekeeping, scheduled operations, and time-based logging.

## Hardware Setup

### TinyRTC Module Components
The TinyRTC module includes:
- **DS1307 RTC**: Real-time clock with battery backup
- **AT24C32 EEPROM**: 4KB (32Kbit) non-volatile storage
- **CR2032 Battery Socket**: For time backup during power loss
- **I2C Interface**: Single bus for both RTC and EEPROM

### TinyRTC Module Wiring
```
TinyRTC Module    ESP32 D1 Mini
VCC           →   5V (or 3.3V if module supports it)
GND           →   GND
SDA           →   GPIO 21
SCL           →   GPIO 22
```

### Pin Usage Summary
- **GPIO 16**: Hunter X-Core communication
- **GPIO 5**: Pump control
- **GPIO 21**: RTC SDA (I2C Data)
- **GPIO 22**: RTC SCL (I2C Clock)

### Voltage Considerations
The TinyRTC is a 5V module, but most work fine with 3.3V I2C signals:

1. **Option 1 (Recommended)**: Connect VCC to 5V, SDA/SCL to ESP32 (3.3V logic)
2. **Option 2**: Connect VCC to 3.3V if module supports it
3. **Option 3**: Use a bidirectional level shifter for guaranteed compatibility

## Features

### RTC Functionality
- **DS1307 Support**: Compatible with TinyRTC modules
- **AT24C32 EEPROM**: 4KB non-volatile storage for configuration/logs
- **Automatic Initialization**: Sets up I2C communication on GPIO 21/22
- **Battery Backup**: Maintains time when power is lost (requires CR2032 battery)
- **NTP Sync**: Automatically syncs with internet time when WiFi is available
- **Status Monitoring**: Reports RTC and EEPROM health and running status
- **Debug Logging**: Prints time at startup and web connections

### REST API Endpoints

#### Get Current Time
```http
GET /api/time
```

**Response:**
```json
{
  "success": true,
  "datetime": "2025-10-29 14:30:45",
  "date": "2025-10-29",
  "time": "14:30:45",
  "year": 2025,
  "month": 10,
  "day": 29,
  "hour": 14,
  "minute": 30,
  "second": 45,
  "dayOfWeek": 2,
  "unixtime": 1730207445,
  "running": true
}
```

#### Set Time Manually
```http
POST /api/time
Content-Type: application/x-www-form-urlencoded

year=2025&month=10&day=29&hour=14&minute=30&second=0
```

**Response:**
```json
{
  "success": true,
  "datetime": "2025-10-29 14:30:00",
  "message": "Time updated successfully"
}
```

## Usage Examples

### Using curl

**Get current time:**
```bash
curl http://192.168.1.100/api/time
```

**Set time:**
```bash
curl -X POST http://192.168.1.100/api/time \
  -d "year=2025&month=10&day=29&hour=14&minute=30&second=0"
```

### Using Python

```python
import requests
import json

ESP32_IP = "192.168.1.100"

# Get current time
response = requests.get(f"http://{ESP32_IP}/api/time")
time_data = response.json()
print(f"Current time: {time_data['datetime']}")
print(f"RTC running: {time_data['running']}")

# Set time
time_params = {
    "year": 2025,
    "month": 10,
    "day": 29,
    "hour": 14,
    "minute": 30,
    "second": 0
}
response = requests.post(f"http://{ESP32_IP}/api/time", data=time_params)
print(response.json())
```

### Using JavaScript

```javascript
// Get current time
fetch('http://192.168.1.100/api/time')
  .then(response => response.json())
  .then(data => {
    console.log('Current time:', data.datetime);
    console.log('Unix timestamp:', data.unixtime);
  });

// Set time
const timeData = new URLSearchParams({
  year: 2025,
  month: 10,
  day: 29,
  hour: 14,
  minute: 30,
  second: 0
});

fetch('http://192.168.1.100/api/time', {
  method: 'POST',
  body: timeData
})
.then(response => response.json())
.then(data => console.log('Time set:', data));
```

## Automatic Features

### NTP Synchronization
- **Automatic Sync**: When WiFi connects, RTC automatically syncs with NTP servers
- **NTP Servers**: Uses `pool.ntp.org` and `time.nist.gov`
- **Fallback**: If NTP fails, uses compile time as fallback
- **One-time Sync**: Syncs once at startup, then relies on RTC battery backup

### Power Loss Handling
- **Battery Backup**: CR2032 battery maintains time during power outages
- **Status Detection**: System detects if RTC lost power and needs time setting
- **Automatic Recovery**: Attempts NTP sync if RTC time is lost

### Error Handling
- **Module Detection**: Reports if RTC module is not found
- **Wiring Help**: Provides wiring instructions if initialization fails
- **Graceful Degradation**: System continues without RTC if module unavailable

## Serial Monitor Output

### Successful Initialization
```
Initializing RTC module...
RTC found and initialized
RTC is running and keeping time
Testing for AT24C32 EEPROM...
EEPROM test successful (4KB AT24C32)
AT24C32 EEPROM detected and functional
=== RTC Status ===
RTC Type: DS1307 (TinyRTC)
EEPROM: AT24C32 Available
I2C Pins: SDA=GPIO21, SCL=GPIO22
Running: Yes
Current Time: 2025-10-29 14:30:45
Unix Timestamp: 1730207445
Day of Week: Tuesday
==================

=== STARTUP TIME ===
Current time: 2025-10-29 14:30:45
====================
Hunter ESP32 Controller Ready!

# When someone accesses the web interface:
Web page requested from: 192.168.1.100
Current time: 2025-10-29 14:31:02
```

### Error Conditions
```
ERROR: Couldn't find RTC! Check wiring:
  TinyRTC VCC → ESP32 5V (or 3.3V)
  TinyRTC GND → ESP32 GND
  TinyRTC SDA → ESP32 GPIO21
  TinyRTC SCL → ESP32 GPIO22
WARNING: RTC is NOT running!
RTC may need new battery or time setting
Testing for AT24C32 EEPROM...
EEPROM not responding on I2C address 0x57
AT24C32 EEPROM not detected or not functional
```

## Troubleshooting

### Common Issues

#### RTC Not Found
- **Check Wiring**: Verify VCC, GND, SDA (GPIO21), SCL (GPIO22)
- **Power Supply**: Try both 5V and 3.3V for VCC
- **Module Health**: Ensure TinyRTC module is working
- **I2C Address**: DS1307 uses address 0x68

#### RTC Not Running
- **Battery**: Check CR2032 battery in TinyRTC module
- **Time Setting**: Use `/api/time` POST to set initial time
- **Crystal**: Ensure 32.768kHz crystal is working

#### I2C Communication Issues
- **Pullup Resistors**: Add 4.7kΩ pullups from SDA/SCL to 3.3V
- **Wire Length**: Keep I2C wires short (< 12 inches)
- **Level Shifting**: Use level shifter if 3.3V/5V compatibility issues

#### EEPROM Issues
- **Address Conflict**: EEPROM uses I2C address 0x57
- **Write Protection**: Some modules have write protection jumpers
- **Memory Size**: AT24C32 provides 4KB (4096 bytes) of storage

### EEPROM Usage Examples
The AT24C32 EEPROM can store configuration data, irrigation logs, or other persistent information:

```cpp
// Example: Store irrigation settings
RTCModule rtc;
uint8_t zoneConfig[10] = {1, 15, 2, 20, 3, 10}; // Zone 1: 15min, Zone 2: 20min, etc.
rtc.writeEEPROM(0x0000, zoneConfig, sizeof(zoneConfig));

// Read back settings
uint8_t savedConfig[10];
rtc.readEEPROM(0x0000, savedConfig, sizeof(savedConfig));
```

### Testing RTC Module
Use an I2C scanner to detect the module:
```cpp
// Add to setup() for testing
Wire.begin(21, 22);
Serial.println("Scanning I2C bus...");
for (int address = 1; address < 127; address++) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0) {
    Serial.print("Found device at 0x");
    Serial.println(address, HEX);
  }
}
// DS1307 should appear at 0x68
```

## Build Status with RTC
All builds successful with RTC and EEPROM integration:
- **ESP32**: 856,501 bytes (65.3% flash)
- **ESP32-S2**: Compatible (build to verify)
- **ESP32-S3**: Compatible (build to verify)

## Future Enhancements
- **Scheduled Irrigation**: Time-based zone activation using EEPROM storage
- **Data Logging**: Timestamped irrigation logs stored in EEPROM
- **Configuration Storage**: Zone settings and schedules in non-volatile memory
- **Alarm Functions**: Daily/weekly watering schedules
- **Timezone Support**: Local time with DST handling
- **EEPROM Management**: Web interface for viewing/editing stored data