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
    String buildScheduleUrl(int days = 1, int8_t zoneId = -1);  // Support multi-day fetch
    String buildCompletionUrl();
    String buildEventStartUrl();
    bool parseScheduleResponse(const String& json, int expectedDays = 1);
    bool parse5DayScheduleResponse(const String& json);
    String createCompletionPayload(const EventCompletion& completion);
    String createEventStartPayload(uint32_t scheduleId, uint8_t zoneId, const String& startTime);
    bool executeRequest(const String& url, String& response);
    bool executePostRequest(const String& url, const String& payload, String& response);

public:
    HTTPScheduleClient();

    // Initialization
    bool begin(ConfigManager* config, ScheduleManager* schedule);
    void setServerUrl(const String& url);
    void setDeviceId(const String& id);

    // Schedule fetching
    bool fetch5DaySchedule(int8_t zoneId = -1);  // Fetch 5-day rolling lookahead
    bool fetchDailySchedule(const String& date, int8_t zoneId = -1);
    bool fetchTodaySchedule();

    // Event completion reporting
    bool reportCompletion(const EventCompletion& completion);
    bool reportCompletion(uint32_t scheduleId, uint8_t zoneId,
                         float durationMin, float waterUsed,
                         const String& status = "completed");

    // Event start notification (immediate update when watering begins)
    bool reportEventStart(uint32_t scheduleId, uint8_t zoneId, const String& startTime);

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
