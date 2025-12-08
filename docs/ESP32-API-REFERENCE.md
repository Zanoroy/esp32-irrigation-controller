# ESP32 Irrigation Controller - API Reference

Complete REST API documentation for the ESP32 Irrigation Controller firmware.

**Base URL**: `http://<ESP32_IP_ADDRESS>/api/`

Example: `http://172.17.98.215/api/`

---

## Table of Contents

- [Zone Control](#zone-control)
- [System Information](#system-information)
- [Time & RTC Management](#time--rtc-management)
- [Configuration](#configuration)
- [Schedule Management](#schedule-management)
- [Device Status](#device-status)
- [Event Logging](#event-logging)
- [MQTT Configuration](#mqtt-configuration)

---

## Zone Control

### Start Zone

**Endpoint**: `GET /api/start-zone`

**Description**: Manually start watering a specific zone for a specified duration.

**Parameters**:
- `zone` (integer, required): Zone number (1-48)
- `time` (integer, required): Duration in minutes (1-240)

**Example Request**:
```bash
curl "http://172.17.98.215/api/start-zone?zone=3&time=15"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Zone 3 started for 15 minutes",
  "zone": 3,
  "duration_minutes": 15,
  "event_id": 123
}
```

**Error Responses**:
- `400 Bad Request`: Missing or invalid parameters
  ```json
  {"status": "error", "message": "Zone must be 1-48"}
  ```
- `403 Forbidden`: Zone not enabled
  ```json
  {"status": "error", "message": "Zone 3 is not enabled. Maximum enabled zones: 8"}
  ```
- `409 Conflict`: Another zone already running
  ```json
  {"status": "error", "message": "Zone 2 is already running"}
  ```

**Notes**:
- Only one zone can run at a time (pump restriction)
- Creates a MANUAL event in the event log
- Automatically stops after the specified duration
- Turns on the pump while zone is active

---

### Stop Zone

**Endpoint**: `GET /api/stop-zone`

**Description**: Manually stop a running zone.

**Parameters**:
- `zone` (integer, required): Zone number (1-48)

**Example Request**:
```bash
curl "http://172.17.98.215/api/stop-zone?zone=3"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Zone 3 stopped",
  "zone": 3
}
```

**Error Responses**:
- `400 Bad Request`: Missing zone parameter
- `403 Forbidden`: Zone not enabled
- `404 Not Found`: Zone was not running

**Notes**:
- Logs event as interrupted (not completed)
- Turns off pump if no other zones are active

---

### Run Program

**Endpoint**: `GET /api/run-program`

**Description**: Execute a pre-programmed watering sequence (legacy Hunter protocol).

**Parameters**:
- `program` (integer, required): Program number (1-4)

**Example Request**:
```bash
curl "http://172.17.98.215/api/run-program?program=1"
```

**Success Response** (200 OK):
```text
Program 1 started
```

**Error Responses**:
- `400 Bad Request`: Invalid program number
  ```text
  ERROR: Program must be 1-4
  ```

---

## System Information

### Get Status

**Endpoint**: `GET /api/status`

**Description**: Retrieve comprehensive system status including ESP32 health, pump state, and RTC time.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/status"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "system": {
    "free_heap": 189672,
    "uptime_seconds": 3600,
    "wifi_rssi": -55,
    "ip_address": "172.17.98.215",
    "mac_address": "20:43:A8:41:1B:64"
  },
  "pump": {
    "status": "OFF"
  },
  "rtc": {
    "status": "connected",
    "local_time": "2025-12-08 15:24:59 (UTC+10:30 DST)"
  }
}
```

**Response Fields**:
- `system.free_heap`: Available RAM in bytes
- `system.uptime_seconds`: Seconds since last boot
- `system.wifi_rssi`: WiFi signal strength (dBm)
- `pump.status`: "ON" or "OFF"
- `rtc.status`: "connected" or "not_available"
- `rtc.local_time`: Current local time with timezone

**Notes**:
- Use this endpoint for health monitoring
- RSSI values: -30 (excellent) to -90 (poor)

---

## Time & RTC Management

### Get Time

**Endpoint**: `GET /api/time`

**Description**: Get current time from the RTC module.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/time"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "time": "2025-12-08 15:30:45 (UTC+10:30 DST)"
}
```

**Error Responses**:
- `500 Internal Server Error`: RTC not available

---

### Sync with NTP

**Endpoint**: `GET /api/sync-ntp`

**Description**: Synchronize RTC with NTP time servers.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/sync-ntp"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "RTC synchronized with NTP time",
  "local_time": "2025-12-08 15:30:45 (UTC+10:30 DST)"
}
```

**Error Responses**:
- `500 Internal Server Error`: WiFi not connected or NTP sync failed
  ```json
  {"status": "error", "message": "WiFi not connected"}
  ```

**Notes**:
- Uses configured NTP servers (default: pool.ntp.org, time.nist.gov)
- Applies configured timezone offset automatically
- RTC stores UTC time internally

---

### Set Time

**Endpoint**: `POST /api/set-time`

**Description**: Manually set the RTC time (not yet implemented).

**Status**: `501 Not Implemented`

---

## Configuration

### Get Configuration

**Endpoint**: `GET /api/config`

**Description**: Retrieve current system configuration.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/config"
```

**Success Response** (200 OK):
```json
{
  "version": 1,
  "timezone": 10.5,
  "daylight_saving": true,
  "ntp_server1": "pool.ntp.org",
  "ntp_server2": "time.nist.gov",
  "auto_ntp": true,
  "sync_interval": 24,
  "mqtt_enabled": true,
  "mqtt_broker": "172.17.254.10",
  "mqtt_port": 1883,
  "mqtt_username": "",
  "mqtt_topic_prefix": "irrigation/",
  "mqtt_retain": true,
  "mqtt_keep_alive": 60,
  "server_enabled": true,
  "server_url": "http://172.17.254.10:2880",
  "device_id": "esp32_irrigation_01",
  "server_retry_interval": 3600,
  "server_max_retries": 24,
  "scheduling": true,
  "max_runtime": 240,
  "max_enabled_zones": 8,
  "pump_safety": true
}
```

---

### Update Configuration

**Endpoint**: `POST /api/config`

**Description**: Update system configuration settings.

**Content-Type**: `application/x-www-form-urlencoded` or `application/json`

**Supported Parameters**:

#### Timezone Settings
- `timezone` (float): Timezone offset in hours (e.g., 10.5 for UTC+10:30)
- `daylight_saving` (boolean): Enable DST adjustment

#### NTP Settings
- `ntp_server1` (string): Primary NTP server
- `ntp_server2` (string): Secondary NTP server
- `auto_ntp` (boolean): Enable automatic NTP sync
- `sync_interval` (integer): Hours between NTP syncs (1-168)

#### MQTT Settings
- `mqtt_enabled` (boolean): Enable MQTT
- `mqtt_broker` (string): MQTT broker hostname/IP
- `mqtt_port` (integer): MQTT port (1-65535)
- `mqtt_username` (string): MQTT username
- `mqtt_password` (string): MQTT password
- `mqtt_topic_prefix` (string): Topic prefix (default: "irrigation/")
- `mqtt_retain` (boolean): Retain MQTT messages
- `mqtt_keep_alive` (integer): Keep-alive seconds (1-300)

#### Server Settings
- `server_enabled` (boolean): Enable schedule server integration
- `server_url` (string): Node-RED server URL
- `device_id` (string): Device identifier
- `server_retry_interval` (integer): Retry interval in seconds (60-86400)
- `server_max_retries` (integer): Max retries (0-100)

#### Irrigation Settings
- `scheduling` (boolean): Enable automatic scheduling
- `max_runtime` (integer): Max zone runtime in minutes (1-1440)
- `max_enabled_zones` (integer): Number of enabled zones (1-16)
- `pump_safety` (boolean): Auto pump shutoff when no zones active

**Example Request (URL Parameters)**:
```bash
curl -X POST "http://172.17.98.215/api/config?timezone=10.5&daylight_saving=true"
```

**Example Request (JSON)**:
```bash
curl -X POST "http://172.17.98.215/api/config" \
  -H "Content-Type: application/json" \
  -d '{
    "timezone": 10.5,
    "daylight_saving": true,
    "scheduling": true,
    "max_enabled_zones": 8
  }'
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Configuration updated successfully",
  "config": { ... }
}
```

**Error Responses**:
- `400 Bad Request`: Invalid parameters
- `500 Internal Server Error`: Failed to save configuration

**Notes**:
- Configuration is saved to ESP32 NVS (non-volatile storage)
- Changes persist across reboots
- JSON body must use URL parameters for float values (timezone)

---

## Schedule Management

### Get All Schedules

**Endpoint**: `GET /api/schedules`

**Description**: Retrieve all configured schedules (both basic and AI).

**Parameters**: None

**CORS**: Enabled (Access-Control-Allow-Origin: *)

**Example Request**:
```bash
curl "http://172.17.98.215/api/schedules"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "count": 3,
  "schedules": [
    {
      "id": 1,
      "zone": 1,
      "start_hour": 0,
      "start_minute": 0,
      "duration_min": 11,
      "days": 127,
      "type": "ai",
      "enabled": true
    },
    {
      "id": 2,
      "zone": 2,
      "start_hour": 22,
      "start_minute": 15,
      "duration_min": 8,
      "days": 127,
      "type": "ai",
      "enabled": true
    },
    {
      "id": 3,
      "zone": 3,
      "start_hour": 22,
      "start_minute": 30,
      "duration_min": 18,
      "days": 127,
      "type": "ai",
      "enabled": true
    }
  ]
}
```

**Response Fields**:
- `days`: Bitmask for days of week (bit 0=Sunday, 127=every day)
- `type`: "basic" or "ai"
- `enabled`: Schedule active status

---

### Create Basic Schedule

**Endpoint**: `POST /api/schedules`

**Description**: Create a new basic (user-defined) schedule.

**Parameters**:
- `zone` (integer, required): Zone number (1-16)
- `hour` (integer, required): Start hour (0-23)
- `minute` (integer, required): Start minute (0-59)
- `duration` (integer, required): Duration in minutes (1-1440)
- `days` (integer, optional): Day bitmask (default: 127 = every day)

**Example Request**:
```bash
curl -X POST "http://172.17.98.215/api/schedules?zone=4&hour=6&minute=30&duration=20&days=62"
```

**Success Response** (201 Created):
```json
{
  "status": "success",
  "message": "Schedule created",
  "schedule_id": 4
}
```

**Error Responses**:
- `400 Bad Request`: Missing or invalid parameters
- `500 Internal Server Error`: Failed to create schedule

**Notes**:
- Day bitmask: Sunday=1, Monday=2, Tuesday=4, ..., Saturday=64
- Example: Weekdays only = 2+4+8+16+32 = 62

---

### Get Active Zones

**Endpoint**: `GET /api/schedules/active`

**Description**: Get currently running zones and their status.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/schedules/active"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "active_zones": [
    {
      "zone": 3,
      "start_time": 1733628900,
      "duration_minutes": 15,
      "remaining_minutes": 8,
      "type": "manual"
    }
  ],
  "pump_active": true
}
```

---

### Set AI Schedules (Batch)

**Endpoint**: `POST /api/schedules/ai`

**Description**: Update all AI-generated schedules from Node-RED server.

**Content-Type**: `application/json`

**Request Body**:
```json
{
  "schedules": [
    {
      "zone": 1,
      "start_hour": 0,
      "start_minute": 0,
      "duration_min": 11,
      "days": 127
    },
    {
      "zone": 2,
      "start_hour": 22,
      "start_minute": 15,
      "duration_min": 8,
      "days": 127
    }
  ]
}
```

**Example Request**:
```bash
curl -X POST "http://172.17.98.215/api/schedules/ai" \
  -H "Content-Type: application/json" \
  -d @schedules.json
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "AI schedules updated"
}
```

**Notes**:
- Replaces all existing AI schedules
- Preserves basic (user-defined) schedules
- Used by Node-RED automation

---

### Clear AI Schedules

**Endpoint**: `DELETE /api/schedules/ai`

**Description**: Remove all AI-generated schedules.

**Parameters**: None

**Example Request**:
```bash
curl -X DELETE "http://172.17.98.215/api/schedules/ai"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "AI schedules cleared"
}
```

**Notes**:
- Does not affect basic schedules
- Useful for troubleshooting or switching back to manual control

---

### Fetch Schedules from Server

**Endpoint**: `POST /api/schedules/fetch`

**Description**: Manually trigger schedule fetch from Node-RED server.

**CORS**: Enabled

**Parameters**:
- `days` (integer, optional): Number of days to fetch (1-5, default: 5)

**Example Request**:
```bash
curl -X POST "http://172.17.98.215/api/schedules/fetch?days=5"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Schedules fetched successfully",
  "days": 5
}
```

**Error Responses**:
- `500 Internal Server Error`: HTTP client not available or fetch failed
  ```json
  {"status": "error", "message": "Schedule fetch failed", "error": "Connection timeout"}
  ```

**Notes**:
- Fetches schedules for next N days
- Automatically called daily at midnight
- Can be triggered manually from web UI

---

## Device Status

### Get Device Status

**Endpoint**: `GET /api/device/status`

**Description**: Get detailed device status for Node-RED integration.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/device/status"
```

**Success Response** (200 OK):
```json
{
  "device_id": "esp32_irrigation_01",
  "timestamp": 1733628900,
  "pump_active": false,
  "active_zones": [],
  "schedule_count": 3,
  "next_event": {
    "zone": 2,
    "time": "22:15",
    "duration": 8
  }
}
```

---

### Get Next Scheduled Event

**Endpoint**: `GET /api/device/next`

**Description**: Get the next upcoming scheduled watering event.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/device/next"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "next_event": {
    "zone": 2,
    "start_time": "22:15",
    "duration_minutes": 8,
    "type": "ai",
    "time_until_minutes": 385
  }
}
```

**No Events Response**:
```json
{
  "status": "success",
  "next_event": null,
  "message": "No upcoming events"
}
```

---

### Send Device Command

**Endpoint**: `POST /api/device/command`

**Description**: Execute device commands from Node-RED.

**Content-Type**: `application/json`

**Request Body**:
```json
{
  "command": "update_schedule",
  "zone": 3,
  "start_hour": 18,
  "start_minute": 0,
  "duration": 25
}
```

**Example Request**:
```bash
curl -X POST "http://172.17.98.215/api/device/command" \
  -H "Content-Type: application/json" \
  -d '{"command": "update_schedule", "zone": 3, "start_hour": 18, "start_minute": 0, "duration": 25}'
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Command executed"
}
```

**Error Responses**:
- `400 Bad Request`: Invalid command
- `405 Method Not Allowed`: Non-POST request

---

## Event Logging

### Get Events

**Endpoint**: `GET /api/events`

**Description**: Retrieve irrigation event logs with optional filtering.

**CORS**: Enabled

**Parameters**:
- `limit` (integer, optional): Max events to return (1-1000, default: 100)
- `start_date` (integer, optional): Unix timestamp for start date filter
- `end_date` (integer, optional): Unix timestamp for end date filter

**Example Request**:
```bash
curl "http://172.17.98.215/api/events?limit=50&start_date=1733097600"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "count": 3,
  "events": [
    {
      "id": 123,
      "zone_id": 2,
      "start_time": 1733628900,
      "end_time": 1733629380,
      "duration_min": 8,
      "completed": true,
      "type": "ai",
      "schedule_id": 2
    },
    {
      "id": 122,
      "zone_id": 3,
      "start_time": 1733542500,
      "end_time": 1733543580,
      "duration_min": 18,
      "completed": true,
      "type": "ai",
      "schedule_id": 3
    }
  ]
}
```

**Response Fields**:
- `type`: "manual", "scheduled", "ai", or "system"
- `completed`: true if zone ran full duration, false if interrupted
- `duration_min`: Planned duration
- `start_time` / `end_time`: Unix timestamps

**Notes**:
- Events stored in SPIFFS (limited by flash memory)
- Recommend periodic cleanup of old events

---

### Clear Events

**Endpoint**: `DELETE /api/events`

**Description**: Clear event logs with optional filtering.

**CORS**: Enabled

**Parameters**:
- `all` (boolean, optional): Clear all events if "true"
- `days` (integer, optional): Keep events from last N days (default: 365)

**Example Request (Clear all)**:
```bash
curl -X DELETE "http://172.17.98.215/api/events?all=true"
```

**Example Request (Keep 30 days)**:
```bash
curl -X DELETE "http://172.17.98.215/api/events?days=30"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "Old events cleared",
  "cleared": 15,
  "kept_days": 30
}
```

---

### Get Event Statistics

**Endpoint**: `GET /api/events/stats`

**Description**: Get statistical analysis of irrigation events.

**CORS**: Enabled

**Parameters**:
- `start_date` (integer, optional): Unix timestamp for start date
- `end_date` (integer, optional): Unix timestamp for end date

**Example Request**:
```bash
curl "http://172.17.98.215/api/events/stats"
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "total_events": 45,
  "completed_events": 43,
  "interrupted_events": 2,
  "total_runtime_minutes": 360,
  "by_zone": {
    "1": {"count": 15, "total_minutes": 120},
    "2": {"count": 15, "total_minutes": 120},
    "3": {"count": 15, "total_minutes": 120}
  },
  "by_type": {
    "ai": 40,
    "manual": 5,
    "scheduled": 0
  }
}
```

---

## MQTT Configuration

### Get MQTT Configuration

**Endpoint**: `GET /api/mqtt/config`

**Description**: Retrieve current MQTT settings.

**Parameters**: None

**Example Request**:
```bash
curl "http://172.17.98.215/api/mqtt/config"
```

**Success Response** (200 OK):
```json
{
  "mqtt_enabled": true,
  "mqtt_broker": "172.17.254.10",
  "mqtt_port": 1883,
  "mqtt_username": "",
  "mqtt_topic_prefix": "irrigation/",
  "mqtt_retain": true,
  "mqtt_keep_alive": 60
}
```

**Notes**:
- Password not included in response for security

---

### Update MQTT Configuration

**Endpoint**: `POST /api/mqtt/config`

**Description**: Update MQTT connection settings.

**Content-Type**: `application/json`

**Request Body**:
```json
{
  "mqtt_enabled": true,
  "mqtt_broker": "192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_username": "irrigation",
  "mqtt_password": "secret123",
  "mqtt_topic_prefix": "home/irrigation/",
  "mqtt_retain": true,
  "mqtt_keep_alive": 60
}
```

**Example Request**:
```bash
curl -X POST "http://172.17.98.215/api/mqtt/config" \
  -H "Content-Type: application/json" \
  -d @mqtt-config.json
```

**Success Response** (200 OK):
```json
{
  "status": "success",
  "message": "MQTT configuration updated"
}
```

**Error Responses**:
- `400 Bad Request`: Invalid JSON
- `405 Method Not Allowed`: Non-POST request

**Notes**:
- Changes saved to NVS
- MQTT connection will reconnect with new settings

---

## CORS Support

The following endpoints support CORS for web application integration:

- `GET /api/schedules`
- `POST /api/schedules/fetch`
- `GET /api/events`
- `DELETE /api/events`
- `GET /api/events/stats`

**CORS Headers**:
```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

**OPTIONS Requests**: Supported for preflight checks

---

## Error Handling

All endpoints return JSON error responses with consistent format:

```json
{
  "status": "error",
  "message": "Description of the error"
}
```

**Common HTTP Status Codes**:
- `200 OK`: Success
- `201 Created`: Resource created successfully
- `400 Bad Request`: Invalid parameters or request body
- `403 Forbidden`: Action not allowed (e.g., zone disabled)
- `404 Not Found`: Resource not found
- `405 Method Not Allowed`: Wrong HTTP method
- `409 Conflict`: Resource conflict (e.g., zone already running)
- `500 Internal Server Error`: Server-side error
- `501 Not Implemented`: Feature not yet implemented

---

## Rate Limiting

**Note**: This firmware does not implement rate limiting. External clients should implement their own throttling to prevent overwhelming the ESP32.

**Recommendations**:
- Status polling: Max 1 request per second
- Event logs: Max 1 request per 5 seconds
- Schedule updates: Avoid rapid consecutive updates

---

## Authentication

**Current Status**: No authentication required

**Security Note**: This device is designed for use on trusted local networks only. For production deployments:
- Use a VPN or firewall to restrict access
- Consider implementing API key authentication
- Use HTTPS reverse proxy for encrypted communication

---

## Examples

### Complete Workflow: Create and Monitor Schedule

```bash
# 1. Check current status
curl "http://172.17.98.215/api/status"

# 2. Get existing schedules
curl "http://172.17.98.215/api/schedules"

# 3. Create new schedule (Zone 4, 6:30 AM, 20 minutes, weekdays only)
curl -X POST "http://172.17.98.215/api/schedules?zone=4&hour=6&minute=30&duration=20&days=62"

# 4. Verify schedule was created
curl "http://172.17.98.215/api/schedules"

# 5. Check next scheduled event
curl "http://172.17.98.215/api/device/next"

# 6. Monitor event logs after execution
curl "http://172.17.98.215/api/events?limit=10"
```

### Manual Zone Control

```bash
# Start zone 3 for 15 minutes
curl "http://172.17.98.215/api/start-zone?zone=3&time=15"

# Check active zones
curl "http://172.17.98.215/api/schedules/active"

# Stop zone early if needed
curl "http://172.17.98.215/api/stop-zone?zone=3"
```

### Configuration Update

```bash
# Update timezone to UTC+10:30 (Adelaide ACDT)
curl -X POST "http://172.17.98.215/api/config?timezone=10.5&daylight_saving=true"

# Sync with NTP to verify
curl "http://172.17.98.215/api/sync-ntp"

# Check updated time
curl "http://172.17.98.215/api/time"
```

---

## Changelog

### Version 1.0 (December 2025)
- Initial API documentation
- All core endpoints documented
- Event logging system
- AI schedule integration
- CORS support for web applications

---

## Support

For issues or questions:
- GitHub Issues: [Your Repository]
- Documentation: See `docs/` folder
- Hardware Info: `docs/README_PLATFORMIO.md`

---

## License

[Your License Information]
