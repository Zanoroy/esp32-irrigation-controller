# Baseline Schedule Fallback Implementation Plan
**ESP32 Irrigation Controller - Offline Resilience Enhancement**

*Date: December 12, 2025*
*Status: Proposal - Pending Management Approval*

---

## Executive Summary

This plan implements a resilient fallback scheduling system for ESP32 irrigation controllers. When network connectivity fails or the server is unavailable, devices will automatically fall back to pre-loaded baseline schedules based on historical weather patterns and plant requirements. This ensures continuous irrigation operations even during extended outages.

**Key Benefits:**
- ✅ 100% uptime for irrigation operations (no crop loss during outages)
- ✅ Automatic failover with zero manual intervention
- ✅ Uses remaining cached daily schedule before falling back (intelligent)
- ✅ Statistical baseline schedules based on historical data
- ✅ Gradual degradation instead of complete failure
- ✅ Automatic recovery when connectivity restored

**Estimated Effort:** 16-20 hours
**Risk Level:** Low
**Dependencies:** None (baseline schedule generation already complete)

---

## 1. Technical Overview

### 1.1 Current Architecture
```
ESP32 → HTTP GET /api/schedules/daily?days=5
      ↓
   Server responds with 5-day optimized schedule
      ↓
   ESP32 executes schedule
```

**Current Limitation:** If server unreachable → **No watering occurs** → Crop damage

### 1.2 Proposed Architecture
```
┌─────────────────────────────────────────────────────┐
│  Server                                             │
│  ┌──────────────────┐    ┌──────────────────┐     │
│  │  Daily Schedule  │    │ Baseline Schedule│     │
│  │  (5-day weather) │    │ (52-week stats)  │     │
│  └──────────────────┘    └──────────────────┘     │
│         API                     API                 │
└─────────┬───────────────────────┬───────────────────┘
          │                       │
          │ Online Mode           │ Initial Load
          ↓                       ↓
    ┌─────────────────────────────────────┐
    │  ESP32 Controller                   │
    │  ┌──────────────┐  ┌──────────────┐│
    │  │ Daily Buffer │  │Baseline Cache││
    │  │  (5 days)    │  │ (52 weeks)   ││
    │  └──────────────┘  └──────────────┘│
    │         ↓                ↓          │
    │    ┌─────────────────────┐         │
    │    │  Schedule Executor  │         │
    │    │  (Smart Fallback)   │         │
    │    └─────────────────────┘         │
    └─────────────────────────────────────┘
```

### 1.3 Fallback Logic
```cpp
Schedule getNextWateringEvent() {
    if (dailyScheduleAvailable && !expired) {
        return getDailySchedule();  // Preferred: weather-optimized
    } else if (baselineScheduleLoaded) {
        return getBaselineSchedule(currentWeek, currentDay);  // Fallback
    } else {
        return emergencySchedule();  // Last resort: minimal safe watering
    }
}
```

---

## 2. Implementation Components

### 2.1 Backend API Enhancements

#### 2.1.1 New Endpoint: GET /api/baseline-schedules/device/:device_id
**Purpose:** Provide compact baseline schedule for ESP32 storage

**Request:**
```http
GET /api/baseline-schedules/device/esp32_irrigation_01?zone_id=1
Authorization: Bearer {device_token}
```

**Response Format:**
```json
{
  "success": true,
  "device_id": "esp32_irrigation_01",
  "zone_id": 1,
  "year": 2025,
  "generated_date": "2025-12-11T10:30:00Z",
  "schedule_version": "v1.2.3",
  "events": [
    {
      "week": 1,
      "day": 1,
      "start_time": "06:00",
      "duration_min": 25,
      "water_mm": 4.2,
      "conditions": {
        "avg_temp": 18.5,
        "avg_precip": 2.3
      }
    }
    // ... 365 events (52 weeks × 7 days, filtered to watering days only)
  ],
  "metadata": {
    "total_events": 156,
    "avg_duration_min": 22,
    "total_yearly_water_mm": 650,
    "compression": "none"
  }
}
```

**Data Size Estimate:**
- Per event: ~120 bytes JSON
- 156 events/year: ~18.7 KB
- 3 zones: ~56 KB
- SPIFFS available: 1.5 MB ✅

**Implementation Effort:** 3 hours
- Node-RED flow: 1.5 hours
- Query optimization: 1 hour
- Testing: 0.5 hours

---

#### 2.1.2 Endpoint Enhancement: GET /api/baseline-schedules/device/:device_id/compressed
**Purpose:** Reduce storage requirements for multi-zone systems

**Compression Strategy:**
```javascript
// Group similar events (within 5 min duration, same time)
// Store once with week range: weeks: [1, 2, 3, 4]
{
  "events": [
    {
      "weeks": [1, 2, 3, 4],  // Applies to weeks 1-4
      "day": 1,
      "start_time": "06:00",
      "duration_min": 25
    }
  ]
}
// Reduction: ~40% smaller (365 → 220 events)
```

**Implementation Effort:** 2 hours

---

### 2.2 ESP32 Firmware Changes

#### 2.2.1 Baseline Schedule Storage (`baseline_schedule.h/cpp`)

**New Classes:**
```cpp
struct BaselineEvent {
    uint8_t week;          // 1-52
    uint8_t dayOfWeek;     // 0=Sunday, 6=Saturday
    uint16_t startMinutes; // Minutes since midnight (0-1439)
    uint16_t durationMin;  // Duration in minutes
    float waterDepthMm;    // Calculated water depth
};

class BaselineScheduleManager {
public:
    bool loadFromServer(const char* deviceId, int zoneId);
    bool loadFromSPIFFS(const char* filename);
    bool saveToSPIFFS(const char* filename);

    BaselineEvent* getEventForDate(int week, int dayOfWeek);
    bool isScheduleValid();
    time_t getLastUpdated();
    String getVersion();

private:
    std::vector<BaselineEvent> events;
    String scheduleVersion;
    time_t lastUpdated;
    bool isValid;
};
```

**Implementation Effort:** 5 hours
- Data structures: 1 hour
- SPIFFS read/write: 2 hours
- HTTP fetch logic: 1.5 hours
- Testing: 0.5 hours

---

#### 2.2.2 Schedule Executor Enhancement (`schedule_manager.cpp`)

**New Logic:**
```cpp
enum ScheduleSource {
    DAILY_OPTIMIZED,      // Server-provided 5-day schedule (preferred)
    BASELINE_FALLBACK,    // Baseline statistical schedule
    EMERGENCY_MINIMAL     // Hardcoded minimal safe schedule
};

class ScheduleManager {
public:
    void update() {
        // Try to fetch fresh daily schedule from server
        if (fetchDailySchedule()) {
            currentSource = DAILY_OPTIMIZED;
            lastSuccessfulFetch = now();
        }

        // Check if we have valid daily events remaining
        if (hasValidDailyEvents()) {
            currentSource = DAILY_OPTIMIZED;
            return;  // Continue using cached schedule
        }

        // Daily schedule expired or empty - fall back to baseline
        if (baselineSchedule.isScheduleValid()) {
            currentSource = BASELINE_FALLBACK;
            Serial.println("⚠️  Daily schedule expired, using baseline fallback");
        } else {
            currentSource = EMERGENCY_MINIMAL;
            Serial.println("🚨 No schedules available, using emergency minimal");
        }
    }

    bool hasValidDailyEvents() {
        // Check if we have any daily events for today or future
        time_t now = time(nullptr);
        for (auto& event : dailySchedule) {
            if (event.timestamp >= now) {
                return true;  // Found future event
            }
        }
        return false;  // All events in the past
    }

    WateringEvent getNextEvent() {
        switch(currentSource) {
            case DAILY_OPTIMIZED:
                return getDailyEvent();
            case BASELINE_FALLBACK:
                return getBaselineEvent();
            case EMERGENCY_MINIMAL:
                return getEmergencyEvent();
        }
    }

private:
    BaselineScheduleManager baselineSchedule;
    ScheduleSource currentSource;
    time_t lastSuccessfulFetch;
    std::vector<WateringEvent> dailySchedule;
};
```

**Implementation Effort:** 4 hours
- Schedule source enum: 0.5 hours
- Fallback logic: 2 hours
- Integration: 1 hour
- Testing: 0.5 hours

---

#### 2.2.3 Baseline Update Strategy

**Approach 1: Periodic Update (Recommended)**
```cpp
// Check for baseline updates once per week
if (WiFi.status() == WL_CONNECTED) {
    if (dayOfWeek == 0 && hour == 3) {  // Sunday 3 AM
        baselineSchedule.loadFromServer(deviceId, zoneId);
        baselineSchedule.saveToSPIFFS("/baseline.json");
    }
}
```

**Approach 2: Version-Based Update**
```cpp
// Check server for new baseline version
String serverVersion = checkBaselineVersion();
if (serverVersion != baselineSchedule.getVersion()) {
    baselineSchedule.loadFromServer(deviceId, zoneId);
    baselineSchedule.saveToSPIFFS("/baseline.json");
}
```

**Implementation Effort:** 2 hours

---

### 2.3 Status Monitoring & Diagnostics

#### 2.3.1 MQTT Status Messages

**New Topic:** `irrigation/{device_id}/status/schedule`
```json
{
  "schedule_source": "baseline_fallback",
  "reason": "server_unreachable",
  "last_server_contact": "2025-12-11T22:15:00Z",
  "baseline_version": "v1.2.3",
  "baseline_loaded": true,
  "next_event": {
    "time": "2025-12-12T06:00:00Z",
    "duration_min": 25,
    "source": "baseline_week_50"
  }
}
```

**Implementation Effort:** 1.5 hours

---

#### 2.3.2 Web Dashboard Indicator

**UI Enhancement:**
```html
<!-- Zone card status indicator -->
<div class="schedule-status">
  <span class="badge badge-warning" ng-if="zone.schedule_source === 'baseline'">
    ⚠️ Baseline Mode
  </span>
  <span class="badge badge-success" ng-if="zone.schedule_source === 'daily'">
    ✓ Optimized
  </span>
  <span class="badge badge-danger" ng-if="zone.schedule_source === 'emergency'">
    🚨 Emergency
  </span>
</div>
```

**Implementation Effort:** 1 hour

---

## 3. Implementation Phases

### Phase 1: Backend API (Week 1, Days 1-2)
**Duration:** 5 hours

- [ ] Create baseline schedule device endpoint
- [ ] Add zone_id filtering
- [ ] Implement JSON response formatting
- [ ] Add compression option
- [ ] Test with curl/Postman
- [ ] Deploy to Node-RED

**Deliverable:** Working API endpoint returning baseline schedules

---

### Phase 2: ESP32 Storage Layer (Week 1, Days 3-4)
**Duration:** 7 hours

- [ ] Create BaselineEvent struct
- [ ] Implement BaselineScheduleManager class
- [ ] Add SPIFFS save/load functions
- [ ] Implement HTTP fetch from server
- [ ] Add validation checks
- [ ] Unit test storage functions

**Deliverable:** ESP32 can fetch and store baseline schedules

---

### Phase 3: Fallback Logic Integration (Week 2, Days 1-2)
**Duration:** 6 hours

- [ ] Add ScheduleSource enum
- [ ] Implement fallback decision logic
- [ ] Add server connectivity timeout
- [ ] Integrate with existing ScheduleManager
- [ ] Add emergency minimal schedule
- [ ] Test fallback transitions

**Deliverable:** ESP32 automatically falls back when server unavailable

---

### Phase 4: Monitoring & Diagnostics (Week 2, Day 3)
**Duration:** 2.5 hours

- [ ] Add MQTT status messages
- [ ] Update web dashboard with status indicators
- [ ] Add logging for fallback events
- [ ] Create alert notifications
- [ ] Test status reporting

**Deliverable:** Operators can see schedule source status

---

### Phase 5: Testing & Documentation (Week 2, Days 4-5)
**Duration:** 4 hours

- [ ] End-to-end testing (normal → fallback → recovery)
- [ ] Network failure simulation tests
- [ ] Multi-day offline testing
- [ ] Performance testing (memory usage)
- [ ] Update system documentation
- [ ] Create operator guide

**Deliverable:** Fully tested and documented system

---

## 4. Testing Strategy

### 4.1 Unit Tests

| Test Case | Description | Success Criteria |
|-----------|-------------|------------------|
| API Response | Fetch baseline from server | Valid JSON, 52 weeks data |
| SPIFFS Storage | Save/load from flash | Data integrity maintained |
| Date Lookup | Get event for week 25, day 3 | Correct event returned |
| Fallback Trigger | Server timeout | Switch to baseline mode |
| Recovery | Server back online | Switch back to daily mode |

### 4.2 Integration Tests

**Scenario 1: Normal Operation → Server Failure → Recovery**
```
1. ESP32 running with daily schedule (normal)
2. Disconnect server network
3. Wait 24 hours (simulated)
4. Verify ESP32 switches to baseline mode
5. Verify watering continues with baseline
6. Reconnect server network
7. Verify ESP32 switches back to daily mode
```

**Scenario 2: Fresh Device with No Daily Schedule**
```
1. New ESP32 powers up
2. No daily schedule available yet
3. Verify ESP32 fetches baseline schedule
4. Verify watering starts using baseline
5. Wait for daily schedule fetch
6. Verify switch to daily when available
```

**Scenario 3: Multi-Day Offline Operation**
```
1. Disconnect from network completely
2. Run for 7 days using baseline only
3. Verify correct events for each day
4. Verify all waterings execute on schedule
5. Reconnect and verify recovery
```

### 4.3 Performance Tests

| Metric | Target | Test Method |
|--------|--------|-------------|
| SPIFFS Storage | < 100 KB per zone | Measure file size |
| Fetch Time | < 10 seconds | Time HTTP request |
| Lookup Time | < 50ms | Benchmark date lookup |
| Memory Usage | < 20 KB RAM | Monitor heap during operation |

---

## 5. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| SPIFFS corruption | Low | High | Regular backups, validation checks |
| Baseline data staleness | Medium | Low | Weekly auto-update, version checking |
| Memory overflow (large schedules) | Low | Medium | Compression, pagination |
| Clock drift during offline | Medium | Medium | Use week-of-year (less sensitive to exact dates) |
| Baseline not matching current conditions | High | Low | Document as "best effort fallback", not optimal |

---

## 6. Rollback Plan

If issues arise during deployment:

1. **ESP32 Firmware:** Flash previous version from backup
2. **API Changes:** Disable new endpoints (non-breaking)
3. **Data:** SPIFFS files can be deleted without impact
4. **Timeline:** < 1 hour to full rollback

**Rollback Triggers:**
- Memory issues causing crashes
- SPIFFS corruption
- Schedule execution failures
- Performance degradation

---

## 7. Success Criteria

### 7.1 Functional Requirements
- ✅ ESP32 automatically fetches baseline schedules on first boot
- ✅ ESP32 stores baselines in SPIFFS persistently
- ✅ Fallback activates when server unreachable > 24 hours
- ✅ Watering continues uninterrupted during outages
- ✅ Automatic recovery when connectivity restored
- ✅ Status visible in web dashboard

### 7.2 Performance Requirements
- ✅ Baseline storage < 100 KB per zone
- ✅ Schedule lookup < 50ms
- ✅ No memory leaks during operation
- ✅ Successful operation for 30 days offline

### 7.3 Quality Requirements
- ✅ Zero crop damage during testing
- ✅ All unit tests passing
- ✅ Integration tests 100% success rate
- ✅ Documentation complete

---

## 8. Resource Requirements

### 8.1 Personnel
- **Senior Developer:** 16-20 hours (firmware + API)
- **QA Tester:** 4 hours (test execution)
- **Technical Writer:** 2 hours (documentation)

### 8.2 Infrastructure
- **Development ESP32:** 1 unit
- **Test Server:** Existing Node-RED instance
- **Test Environment:** Isolated network segment

### 8.3 Tools & Dependencies
- PlatformIO (existing)
- Node-RED (existing)
- MQTT broker (existing)
- ArduinoJson library (existing)

---

## 9. Timeline Summary

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1: Backend API | 5 hours | 5 hours |
| Phase 2: ESP32 Storage | 7 hours | 12 hours |
| Phase 3: Fallback Logic | 6 hours | 18 hours |
| Phase 4: Monitoring | 2.5 hours | 20.5 hours |
| Phase 5: Testing | 4 hours | 24.5 hours |
| **Buffer (20%)** | **4.5 hours** | **29 hours** |

**Total Estimated Effort:** 24-29 hours (3-4 working days)

---

## 10. Post-Implementation Monitoring

### First 30 Days
- Daily review of fallback activation logs
- Weekly baseline update verification
- Memory usage trending
- Schedule execution success rate

### Ongoing
- Monthly review of fallback frequency
- Quarterly baseline schedule regeneration
- Annual system resilience audit

---

## 11. Future Enhancements (Not in Scope)

1. **Machine Learning Baseline Refinement**
   - Use actual watering outcomes to improve baselines
   - Estimated effort: 40 hours

2. **Peer-to-Peer Schedule Sharing**
   - ESP32 devices share schedules in mesh network
   - Estimated effort: 60 hours

3. **Solar/Battery Optimization**
   - Adjust baselines for power availability
   - Estimated effort: 20 hours

---

## 12. Approval & Sign-Off

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Project Manager | ___________ | ___________ | ___/___/___ |
| Technical Lead | ___________ | ___________ | ___/___/___ |
| Operations Manager | ___________ | ___________ | ___/___/___ |

---

## Appendix A: API Request/Response Examples

### Example 1: Fetch Baseline Schedule
```bash
curl -X GET "http://172.17.254.10:2880/api/baseline-schedules/device/esp32_irrigation_01?zone_id=1" \
  -H "Authorization: Bearer device_token_here"
```

### Example 2: Check Baseline Version
```bash
curl -X GET "http://172.17.254.10:2880/api/baseline-schedules/device/esp32_irrigation_01/version?zone_id=1"
```

Response:
```json
{
  "version": "v1.2.3",
  "generated_date": "2025-12-11T10:30:00Z",
  "events_count": 156,
  "size_bytes": 18750
}
```

---

## Appendix B: ESP32 Memory Analysis

**Current Memory Usage:**
- Program: 1.2 MB / 1.9 MB (63%)
- SPIFFS: 0.3 MB / 1.5 MB (20%)
- Heap: 45 KB / 320 KB (14%)

**Projected After Implementation:**
- Program: +50 KB → 1.25 MB / 1.9 MB (66%) ✅
- SPIFFS: +60 KB → 0.36 MB / 1.5 MB (24%) ✅
- Heap: +15 KB → 60 KB / 320 KB (19%) ✅

**Conclusion:** Adequate resources available

---

*End of Implementation Plan*

**Contact:** bruce.quinton@example.com
**Project:** ESP32 Smart Irrigation System
**Document Version:** 1.0
