# ESP32-Server Integration Implementation Plan

**Created**: December 4, 2025  
**Status**: Phase 1 - In Progress  
**Integration Approach**: Option 2 - Hybrid HTTP + MQTT

---

## System Configuration

### Network Topology
- **Node-RED Server**: 172.17.254.10:2880
- **MQTT Broker**: 172.17.254.10:1883 (Mosquitto)
- **ESP32 Controller**: 172.17.98.x (WiFi network)
- **Network**: Cross-subnet (17.254.x → 17.98.x)

### Technology Stack
- **Server**: Node-RED + SQLite + MQTT
- **ESP32**: C++, PlatformIO, Arduino framework
- **Communication**: HTTP (schedule fetch) + MQTT (real-time status)

---

## Implementation Phases

### ✅ Phase 0: Planning & Design (COMPLETED)
- [x] Architecture review
- [x] API endpoint design
- [x] Memory analysis
- [x] Integration strategy selection
- [x] Network configuration confirmed

---

### 🔄 Phase 1: Foundation - Basic HTTP Schedule Fetching (IN PROGRESS)
**Goal**: Establish HTTP communication for daily schedule retrieval

**Timeline**: Week 1  
**Started**: December 4, 2025

#### Server Side Tasks
- [ ] Create REST endpoint: `GET /api/schedules/daily`
  - Query params: `date` (YYYY-MM-DD), `zone_id` (optional)
  - Returns: JSON schedule for ESP32 consumption
- [ ] Test endpoint with curl/Postman
- [ ] Document API format in API.md

#### ESP32 Side Tasks
- [ ] Create `http_client.h` and `http_client.cpp` modules
  - HTTPClient wrapper for schedule fetching
  - JSON parsing with ArduinoJson
  - Error handling and retry logic
- [ ] Add server configuration to ConfigManager
  - Server URL: http://172.17.254.10:2880
  - Connection timeout settings
  - Retry parameters
- [ ] Enhance ScheduleManager
  - Add `loadServerSchedule()` method
  - Parse JSON schedule format
  - Store in existing schedule structures
- [ ] Create daily fetch task
  - RTC-triggered at 6:15 AM
  - Fetch schedule for current day
  - Load into ScheduleManager

#### Testing Checklist
- [ ] Mock schedule JSON parsing (serial monitor)
- [ ] HTTP fetch test with real server
- [ ] Schedule execution test
- [ ] Memory usage verification (heap monitoring)
- [ ] Error handling (server down, network loss)

#### Expected Deliverables
- Working HTTP client module
- Daily schedule auto-fetch at 6:15 AM
- Schedule execution from server data
- Serial logging of all operations

---

### 📋 Phase 2: Integration - Full Sync & Execution (PLANNED)
**Goal**: Complete schedule lifecycle with completion reporting

**Timeline**: Week 2

#### Server Side Tasks
- [ ] Create REST endpoint: `POST /api/events/completion`
  - Accept: schedule_id, zone_id, duration, water_used, status
  - Store in database event logs
- [ ] Add Node-RED flow for schedule broadcast
- [ ] Implement ESP32 request logging
- [ ] Create dashboard widgets for ESP32 monitoring

#### ESP32 Side Tasks
- [ ] Implement SPIFFS schedule caching
  - Save fetched schedules to flash
  - Cache format: `/schedules/YYYY-MM-DD.json`
  - Implement cache cleanup (7-day retention)
- [ ] Add offline mode detection
  - Fallback to cached schedule
  - Fallback to local basic schedules
  - Status indicators for mode
- [ ] Implement completion reporting
  - POST to server after zone completion
  - Include actual duration, water used
  - Retry queue for failed reports
- [ ] Create RTC-based daily update task
  - Scheduled fetch at 6:15 AM
  - Automatic retry on failure
  - Status LED indicators

#### Testing Checklist
- [ ] End-to-end: fetch → execute → report
- [ ] Offline mode (server unavailable)
- [ ] Cache fallback mechanism
- [ ] Multi-zone schedule execution
- [ ] Water usage tracking accuracy
- [ ] Network interruption recovery

#### Expected Deliverables
- Bidirectional communication (fetch + report)
- Offline resilience via caching
- Complete event logging
- Production-ready error handling

---

### 📡 Phase 3: MQTT Real-Time Integration (PLANNED)
**Goal**: Live monitoring, control, and status updates

**Timeline**: Week 3

#### MQTT Topic Structure
```
irrigation/
  ├── schedule/
  │   ├── daily/broadcast         (server → ESP32)
  │   └── update/[device_id]      (server → ESP32)
  ├── command/
  │   ├── zone/start              (server → ESP32)
  │   ├── zone/stop               (server → ESP32)
  │   └── emergency/stop_all      (server → ESP32)
  ├── status/
  │   ├── [device_id]/online      (ESP32 → server)
  │   ├── [device_id]/zone        (ESP32 → server)
  │   └── [device_id]/heartbeat   (ESP32 → server)
  └── config/
      └── [device_id]/update      (server → ESP32)
```

#### Server Side Tasks
- [ ] Configure MQTT broker in Node-RED settings
- [ ] Create MQTT publish flows:
  - Daily schedule broadcast at 6:00 AM
  - Configuration update pushes
  - Zone control commands
- [ ] Create MQTT subscribe flows:
  - ESP32 status monitoring
  - Heartbeat tracking
  - Event completion messages
- [ ] Add MQTT dashboard widgets

#### ESP32 Side Tasks
- [ ] Enhance existing MQTTManager:
  - Subscribe to `irrigation/schedule/daily/broadcast`
  - Subscribe to `irrigation/command/#`
  - Publish to `irrigation/status/[device_id]/#`
  - Add schedule reception via MQTT
- [ ] Implement command handlers:
  - Remote zone start/stop
  - Emergency stop all
  - Configuration updates
- [ ] Add status publishing:
  - Zone state changes
  - Completion events
  - Heartbeat (every minute)
  - Error conditions
- [ ] Implement MQTT-based schedule updates
  - Receive and parse schedule JSON
  - Update ScheduleManager in real-time
  - Acknowledge receipt

#### Testing Checklist
- [ ] MQTT connection reliability
- [ ] Real-time status updates
- [ ] Remote zone control
- [ ] Emergency stop response time
- [ ] Schedule push updates
- [ ] Reconnection handling
- [ ] Message persistence

#### Expected Deliverables
- Real-time bi-directional MQTT communication
- Live dashboard monitoring
- Remote control capabilities
- Sub-second emergency response

---

### 🚀 Phase 4: Advanced Features (PLANNED)
**Goal**: Production-ready reliability and advanced features

**Timeline**: Week 4

#### Server Side Tasks
- [ ] Weather-based override system
  - Rain detection → cancel schedule
  - High wind → delay schedule
  - Forecast integration
- [ ] Multi-device support
  - Device registry
  - Per-device scheduling
  - Load balancing
- [ ] Advanced dashboard
  - Historical charts
  - Water usage analytics
  - Weather correlation graphs
  - Zone performance metrics
- [ ] Alert system
  - Email notifications
  - SMS alerts (optional)
  - Webhook integrations

#### ESP32 Side Tasks
- [ ] Rain sensor integration
  - Hardware rain sensor support
  - Server rain delay override
  - Local rain delay logic
- [ ] Advanced error recovery
  - Exponential backoff retry
  - Circuit breaker pattern
  - Graceful degradation
- [ ] OTA firmware updates
  - Server-triggered updates
  - Version management
  - Rollback capability
- [ ] Enhanced diagnostics
  - Memory leak detection
  - Performance metrics
  - Network quality monitoring
  - Self-diagnostic reports

#### Testing Checklist
- [ ] 7-day continuous operation test
- [ ] Network failure scenarios
- [ ] Server maintenance scenarios
- [ ] Multiple ESP32 devices
- [ ] Weather override accuracy
- [ ] OTA update reliability
- [ ] Memory stability over time

#### Expected Deliverables
- Production-ready system
- 99%+ uptime capability
- Advanced monitoring
- Remote diagnostics

---

## API Endpoint Specifications

### GET /api/schedules/daily

**Purpose**: Fetch daily watering schedule for ESP32

**Request**:
```
GET http://172.17.254.10:2880/api/schedules/daily?date=2025-12-04&zone_id=1
```

**Query Parameters**:
- `date` (required): YYYY-MM-DD format
- `zone_id` (optional): Filter by zone, omit for all zones

**Response** (200 OK):
```json
{
  "success": true,
  "date": "2025-12-04",
  "generated_at": "2025-12-04T06:00:00Z",
  "zones": [
    {
      "zone_id": 1,
      "zone_name": "Front Lawn",
      "events": [
        {
          "id": 123,
          "start_time": "22:00",
          "duration_min": 15,
          "repeat_count": 1,
          "rest_time_min": 0,
          "water_amount_l": 30.0,
          "priority": 5
        }
      ]
    }
  ]
}
```

**Error Response** (404):
```json
{
  "success": false,
  "error": "No schedule found for date 2025-12-04"
}
```

**ESP32 Memory Estimate**: ~2-3KB per day

---

### POST /api/events/completion

**Purpose**: Report watering event completion from ESP32

**Request**:
```
POST http://172.17.254.10:2880/api/events/completion
Content-Type: application/json

{
  "schedule_id": 123,
  "zone_id": 1,
  "device_id": "esp32_irrigation_001",
  "start_time": "2025-12-04T22:00:00Z",
  "end_time": "2025-12-04T22:15:30Z",
  "actual_duration_min": 15.5,
  "water_used_liters": 31.2,
  "status": "completed",
  "notes": ""
}
```

**Response** (200 OK):
```json
{
  "success": true,
  "message": "Event logged successfully",
  "event_id": 456
}
```

---

## Memory Budget (ESP32)

### Current Usage (Baseline)
```
Program Storage:    ~450KB / 1.3MB (35%)
Dynamic Memory:     ~80KB / 320KB (25%)
SPIFFS:            ~0KB / 1.5MB (0%)
```

### Phase 1 Additions
```
HTTPClient code:    ~8KB
JSON parsing:       ~5KB
Schedule storage:   ~2KB
Runtime heap:       ~10KB
Total:             ~25KB (acceptable)
```

### Phase 2 Additions
```
SPIFFS cache:       ~20KB (7 days × 3KB)
Completion queue:   ~2KB
Additional code:    ~5KB
Total:             ~27KB (acceptable)
```

### Phase 3 Additions
```
Enhanced MQTT:      ~3KB
Additional topics:  ~2KB
Status buffers:     ~1KB
Total:             ~6KB (minimal)
```

### Total Integration Impact
```
Code size:          ~20KB
Runtime heap:       ~15KB
SPIFFS storage:     ~20KB
Total overhead:     ~55KB (well within limits)
```

---

## Risk Assessment & Mitigation

### Risk: Network Cross-Subnet Issues
- **Likelihood**: Medium
- **Impact**: High
- **Mitigation**: 
  - Add connection timeout (5 seconds)
  - Implement exponential backoff
  - Fall back to cached schedules
  - Log all network errors

### Risk: ESP32 Memory Overflow
- **Likelihood**: Low
- **Impact**: High
- **Mitigation**:
  - Limit schedule cache to 7 days
  - Monitor heap usage in logs
  - Implement JSON streaming parser
  - Add memory guards

### Risk: Server Downtime
- **Likelihood**: Low
- **Impact**: Medium
- **Mitigation**:
  - Multi-day schedule caching
  - Local basic schedule fallback
  - Offline mode detection
  - Retry queue for reports

### Risk: Time Synchronization Drift
- **Likelihood**: Medium
- **Impact**: Medium
- **Mitigation**:
  - Daily NTP sync
  - RTC battery backup
  - Time drift monitoring
  - Server time validation

---

## Success Criteria

### Phase 1
- ✅ ESP32 successfully fetches daily schedule via HTTP
- ✅ JSON parsing works without memory errors
- ✅ Schedule loads into ScheduleManager
- ✅ Zones execute at correct times
- ✅ Serial logging shows all operations

### Phase 2
- ✅ Completion events POST to server
- ✅ Offline mode works with cached schedules
- ✅ 7-day cache retention operational
- ✅ Error recovery demonstrated
- ✅ Water usage tracking accurate

### Phase 3
- ✅ MQTT connection stable for 24+ hours
- ✅ Real-time status updates < 1 second
- ✅ Remote commands work reliably
- ✅ Emergency stop < 500ms response
- ✅ Schedule updates via MQTT

### Phase 4
- ✅ 7-day continuous operation
- ✅ Multiple ESP32 devices supported
- ✅ Weather overrides working
- ✅ OTA updates successful
- ✅ Production deployment ready

---

## Development Notes

### Testing Environments
1. **Development**: Local bench setup with mock zones
2. **Staging**: Single zone with real hardware
3. **Production**: Full 16-zone deployment

### Version Control
- Tag each phase completion: `v1.1-phase1`, `v1.2-phase2`, etc.
- Branch strategy: `main` (stable), `develop` (integration)

### Documentation Updates
- Update API.md with new endpoints
- Update README.md with integration instructions
- Create ESP32_INTEGRATION.md with setup guide

---

## Change Log

### December 4, 2025
- Created integration plan
- Confirmed network configuration
- Selected Option 2 (Hybrid HTTP + MQTT)
- Starting Phase 1 implementation

---

## Next Session Checklist

When resuming work:
1. Check current phase status in this document
2. Review completed tasks (marked with ✅)
3. Continue with next uncompleted task
4. Update progress markers
5. Test incrementally

---

**Last Updated**: December 4, 2025  
**Current Phase**: Phase 1 - Foundation  
**Next Milestone**: HTTP schedule fetching working
