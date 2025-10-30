# Node-RED ESP32 Irrigation Monitor Flows

## Overview
These Node-RED flows monitor your ESP32 irrigation controller via MQTT and store all data in a SQLite3 database for analysis and historical tracking.

## Features
- **Real-time Monitoring**: Subscribes to all ESP32 MQTT topics
- **Database Storage**: Stores device config, status, zone activity, and schedules
- **Device Discovery**: Automatically captures device IP and configuration
- **Historical Data**: Maintains complete irrigation activity history
- **MQTT Commands**: Send commands back to the ESP32 controller

## Prerequisites
1. **Node-RED** installed on 172.17.254.10
2. **Required Node-RED Nodes**:
   - `node-red-node-sqlite` (for SQLite3 database)
   - `node-red-contrib-mqtt-broker` (usually pre-installed)

Install missing nodes via Node-RED Palette Manager or command line:
```bash
cd ~/.node-red
npm install node-red-node-sqlite
```

## Installation Instructions

### 1. Import the Flows
1. Open Node-RED at `http://172.17.254.10:1880`
2. Go to Menu → Import
3. Copy and paste the contents of `node-red-flows.json`
4. Click "Import"

### 2. Configure Database Path (Optional)
The flows use `/data/irrigation.db` by default. To change:
1. Double-click the "irrigation_db" configuration node
2. Update the database path as needed
3. Click "Done" and "Deploy"

### 3. Deploy the Flows
1. Click the "Deploy" button in Node-RED
2. The database will be automatically initialized on first run

### 4. Verify MQTT Connection
The MQTT broker is configured for:
- **Host**: 172.17.254.10
- **Port**: 1883
- **Client ID**: nodered_irrigation

## Database Schema

### device_config
Stores ESP32 device configuration and network info:
```sql
CREATE TABLE device_config (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT UNIQUE,
    client_id TEXT,
    ip_address TEXT,
    mac_address TEXT,
    wifi_ssid TEXT,
    wifi_rssi INTEGER,
    heap_free INTEGER,
    uptime INTEGER,
    firmware_version TEXT,
    mqtt_broker TEXT,
    mqtt_port INTEGER,
    topic_prefix TEXT,
    timezone REAL,
    max_zones INTEGER,
    timestamp TEXT
);
```

### device_status
Stores general device status updates:
```sql
CREATE TABLE device_status (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    status_data TEXT,  -- JSON data
    timestamp TEXT
);
```

### zone_status
Stores individual zone activity:
```sql
CREATE TABLE zone_status (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    zone_number INTEGER,
    status TEXT,
    time_remaining INTEGER,
    timestamp TEXT
);
```

### schedule_status
Stores irrigation schedule information:
```sql
CREATE TABLE schedule_status (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    schedule_data TEXT,  -- JSON data
    timestamp TEXT
);
```

## MQTT Topics Monitored

The flows automatically subscribe to:
- `irrigation/esp32_irrigation/config/device` - Device configuration
- `irrigation/esp32_irrigation/status/device` - Device status
- `irrigation/esp32_irrigation/status/zone/+` - Zone status (all zones)
- `irrigation/esp32_irrigation/status/schedules` - Schedule status

## Usage

### Monitor Real-time Data
1. Open the Node-RED debug panel (bug icon on right sidebar)
2. Watch incoming MQTT messages and database storage confirmations

### Query Current Device Status
1. Click the "Get Current Device Status" inject node
2. View the formatted response in the debug panel

### Send Commands to ESP32
1. Use the "Send Test Command" inject node as an example
2. Modify the topic and payload for different commands:
   - Topic: `irrigation/esp32_irrigation/command/status`
   - Payload: `request`

### Database Queries
Access the SQLite database directly for custom queries:
```sql
-- Get latest device info
SELECT * FROM device_config ORDER BY timestamp DESC LIMIT 1;

-- Get recent zone activity
SELECT * FROM zone_status ORDER BY timestamp DESC LIMIT 50;

-- Get irrigation history for today
SELECT * FROM zone_status
WHERE date(timestamp) = date('now')
ORDER BY timestamp;
```

## Configuration Options

### MQTT Broker Settings
To change the MQTT broker (currently 172.17.254.10):
1. Double-click the "MQTT Broker" configuration node
2. Update the broker IP address
3. Save and deploy

### Database Location
To change database path from `/data/irrigation.db`:
1. Double-click the "irrigation_db" configuration node
2. Update the path
3. Save and deploy

### Topic Prefix
If your ESP32 uses a different topic prefix:
1. Edit the MQTT input nodes
2. Update topic patterns from `irrigation/esp32_irrigation/...`
3. Save and deploy

## Troubleshooting

### MQTT Connection Issues
- Verify MQTT broker is running on 172.17.254.10:1883
- Check Node-RED debug panel for connection errors
- Ensure ESP32 is connected and publishing

### Database Issues
- Check file permissions for database path
- Verify `node-red-node-sqlite` is installed
- Check Node-RED logs for SQLite errors

### No Data Appearing
- Verify ESP32 is publishing to correct topics
- Check MQTT topic patterns match ESP32 configuration
- Enable debug nodes to see incoming messages

## Data Export

Export data for analysis:
```sql
-- Export zone activity as CSV
.mode csv
.output zone_activity.csv
SELECT device_id, zone_number, status, time_remaining, timestamp
FROM zone_status ORDER BY timestamp;
```

## Security Notes
- Database contains device network information
- Consider encrypting the database file
- Restrict access to Node-RED interface
- Use MQTT authentication if needed