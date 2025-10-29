# Hunter WiFi Remote - REST API Documentation

## Overview
The Hunter WiFi Remote now includes REST API endpoints for programmatic control of sprinkler zones. These endpoints accept JSON payloads and return JSON responses, making them perfect for automation systems, mobile apps, or integration with home automation platforms.

## Base URL
```
http://[ESP32_IP_ADDRESS]/api/
```

## Endpoints

### 1. Start Zone
**Endpoint:** `POST /api/startzone`

**Description:** Starts a specific sprinkler zone for a specified duration.

**Request Format:**
```http
POST /api/startzone
Content-Type: application/x-www-form-urlencoded

zone=1&time=15
```

**Parameters:**
- `zone` (required): Zone number (1-48)
- `time` (required): Duration in minutes (1-240)

**Success Response:**
```json
{
  "success": true,
  "zone": 1,
  "time": 15,
  "action": "started"
}
```

**Error Response:**
```json
{
  "error": "Invalid zone number. Must be 1-48"
}
```

### 2. Stop Zone
**Endpoint:** `POST /api/stopzone`

**Description:** Stops a specific sprinkler zone immediately.

**Request Format:**
```http
POST /api/stopzone
Content-Type: application/x-www-form-urlencoded

zone=1
```

**Parameters:**
- `zone` (required): Zone number (1-48)

**Success Response:**
```json
{
  "success": true,
  "zone": 1,
  "action": "stopped"
}
```

**Error Response:**
```json
{
  "error": "Invalid zone number. Must be 1-48"
}
```

## Usage Examples

### Using curl

**Start Zone 3 for 20 minutes:**
```bash
curl -X POST http://192.168.1.100/api/startzone \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "zone=3&time=20"
```

**Stop Zone 3:**
```bash
curl -X POST http://192.168.1.100/api/stopzone \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "zone=3"
```

### Using Python requests

```python
import requests

# ESP32 IP address
ESP32_IP = "192.168.1.100"

# Start zone 5 for 30 minutes
response = requests.post(f"http://{ESP32_IP}/api/startzone",
                        data={"zone": 5, "time": 30})
print(response.json())

# Stop zone 5
response = requests.post(f"http://{ESP32_IP}/api/stopzone",
                        data={"zone": 5})
print(response.json())
```

### Using JavaScript/Node.js

```javascript
const axios = require('axios');

const ESP32_IP = "192.168.1.100";

// Start zone 2 for 10 minutes
axios.post(`http://${ESP32_IP}/api/startzone`,
  new URLSearchParams({ zone: 2, time: 10 }))
  .then(response => console.log(response.data))
  .catch(error => console.error(error.response.data));

// Stop zone 2
axios.post(`http://${ESP32_IP}/api/stopzone`,
  new URLSearchParams({ zone: 2 }))
  .then(response => console.log(response.data))
  .catch(error => console.error(error.response.data));
```

## Error Handling

### HTTP Status Codes
- `200 OK`: Request successful
- `400 Bad Request`: Invalid parameters or missing required fields
- `404 Not Found`: Endpoint not found

### Common Error Messages
- `"Invalid zone number. Must be 1-48"`: Zone parameter outside valid range
- `"Invalid time. Must be 1-240 minutes"`: Time parameter outside valid range
- `"Missing required parameters: zone and time"`: StartZone missing parameters
- `"Missing required parameter: zone"`: StopZone missing zone parameter

## Integration Notes

### Home Assistant Integration
```yaml
# Example Home Assistant REST command configuration
rest_command:
  start_sprinkler_zone_1:
    url: "http://192.168.1.100/api/startzone"
    method: POST
    headers:
      Content-Type: "application/x-www-form-urlencoded"
    payload: "zone=1&time=15"

  stop_sprinkler_zone_1:
    url: "http://192.168.1.100/api/stopzone"
    method: POST
    headers:
      Content-Type: "application/x-www-form-urlencoded"
    payload: "zone=1"
```

### NodeRED Integration
Use HTTP Request nodes with:
- Method: POST
- URL: `http://[ESP32_IP]/api/startzone` or `/api/stopzone`
- Payload: `{"zone": 1, "time": 15}` (for form data format)

## Hardware Behavior

### Zone Start
1. Validates zone (1-48) and time (1-240)
2. Activates pump (GPIO5 HIGH)
3. Sends Hunter protocol command to start zone
4. Returns success response

### Zone Stop
1. Validates zone (1-48)
2. Deactivates pump (GPIO5 LOW)
3. Sends Hunter protocol command with 0 time (stop)
4. Returns success response

## Compatibility
- Works alongside existing web interface
- Compatible with all ESP32 variants (ESP32, ESP32-S2, ESP32-S3)
- Maintains all existing Hunter X-Core protocol functionality
- Thread-safe with proper interrupt handling during protocol transmission

## Security Notes
- No authentication currently implemented
- Consider network security (firewall, VPN, etc.)
- API is accessible to any device on the same network
- Future versions may include API key authentication