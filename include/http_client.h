#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Forward declarations
class ConfigManager;
class ScheduleManager;

// Server schedule event structure (optimized for ESP32 memory)
struct ServerScheduleEvent {
    uint32_t serverId;        // Server-side schedule ID
    uint8_t zoneId;           // Zone number (1-16)
    uint8_t startHour;        // Start hour (0-23)
    uint8_t startMinute;      // Start minute (0-59)
    uint16_t durationMin;     // Duration in minutes
    uint8_t repeatCount;      // Number of repetitions
    uint16_t restTimeMin;     // Rest time between repetitions
    uint8_t priority;         // Priority (1-10)
    float waterAmountL;       // Expected water amount in liters
};

// Event completion data structure
struct EventCompletion {
    uint32_t scheduleId;      // Server schedule ID
    uint8_t zoneId;           // Zone that was watered
    String deviceId;          // ESP32 device identifier
    String startTime;         // ISO 8601 timestamp
    String endTime;           // ISO 8601 timestamp
    float actualDurationMin;  // Actual duration in minutes
    float waterUsedLiters;    // Actual water used
    String status;            // "completed", "failed", "cancelled"
    String notes;             // Optional notes
};

class HTTPScheduleClient {
private:
    ConfigManager* configManager;
    ScheduleManager* scheduleManager;
    HTTPClient http;

    String serverUrl;         // Base URL: http://172.17.254.10:2880
    String deviceId;          // Unique device identifier

    // HTTP configuration
    static const int HTTP_TIMEOUT = 10000;      // 10 second timeout (cross-subnet)
    static const int MAX_RETRIES = 3;           // Retry failed requests
    static const int RETRY_DELAY = 2000;        // 2 seconds between retries

    // Helper methods
    String buildScheduleUrl(const String& date, int8_t zoneId = -1);  // Build URL with date parameter
    String buildZoneDetailsUrl();
    String buildCompletionUrl();
    String buildEventStartUrl();
    String buildEventSyncUrl();
    bool parseScheduleResponse(const String& json, int expectedDays = 1);
    bool parse5DayScheduleResponse(const String& json);
    bool parseZoneDetailsResponse(const String& json);
    String createCompletionPayload(const EventCompletion& completion);
    String createEventStartPayload(uint32_t scheduleId, uint8_t zoneId, const String& startTime);
    bool executeRequest(const String& url, String& response);
    bool executePostRequest(const String& url, const String& payload, String& response);

    // Zone details cache (keyed by ESP32-facing zone_id/device_zone_number)
    static const uint8_t MAX_ZONE_ID = 48;
    bool zoneHasData[MAX_ZONE_ID + 1];
    String zoneNames[MAX_ZONE_ID + 1];
    bool zoneActive[MAX_ZONE_ID + 1];
    float zoneWaterRateLpm[MAX_ZONE_ID + 1];
    uint32_t zoneDatabaseId[MAX_ZONE_ID + 1];
    uint8_t zoneDetailsCount;
    unsigned long lastZoneDetailsFetchTime;

public:
    HTTPScheduleClient();

    // Initialization
    bool begin(ConfigManager* config, ScheduleManager* schedule);
    void setServerUrl(const String& url);
    void setDeviceId(const String& id);

    // Schedule fetching
    bool fetchSchedule(int days = 1, int8_t zoneId = -1);  // Unified fetch method (1-5 days)
    bool fetch5DaySchedule(int8_t zoneId = -1);  // Fetch 5-day rolling lookahead
    bool fetchDailySchedule(const String& date, int8_t zoneId = -1);
    bool fetchTodaySchedule();

    // Zone details fetching
    bool fetchZoneDetails();
    String getZoneName(uint8_t zoneId) const;
    bool hasZoneDetails(uint8_t zoneId) const;
    String getZoneDetailsJSON() const;  // {count, last_fetch_ms, zones:[...]}
    unsigned long getLastZoneDetailsFetchTime() const { return lastZoneDetailsFetchTime; }

    // SPIFFS caching for offline resilience
    bool cacheScheduleToSPIFFS(const String& date, const String& json);
    bool loadScheduleFromCache(const String& date);
    bool loadLatestCachedSchedule();
    bool clearOldCache(int daysToKeep = 7);

    // Event completion reporting
    bool reportCompletion(const EventCompletion& completion);
    bool reportCompletion(uint32_t scheduleId, uint8_t zoneId,
                         float durationMin, float waterUsed,
                         const String& status = "completed");

    // Event start notification (immediate update when watering begins)
    bool reportEventStart(uint32_t scheduleId, uint8_t zoneId, const String& startTime);

    // Event synchronization (bulk upload after offline period)
    bool syncPendingEvents();
    bool savePendingEvent(const EventCompletion& completion);
    int getPendingEventCount();

    // Status and diagnostics
    bool testConnection();
    String getLastError() const { return lastError; }
    unsigned long getLastFetchTime() const { return lastFetchTime; }

    // Retry tracking
    int getConsecutiveFailures() const { return consecutiveFailures; }
    void resetFailureCount() { consecutiveFailures = 0; }

private:
    String lastError;
    unsigned long lastFetchTime;
    int consecutiveFailures;  // Track consecutive fetch failures for retry logic
};

#endif // HTTP_CLIENT_H
