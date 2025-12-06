#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>

// Event types
enum class EventType {
    MANUAL,      // Manual start via API or MQTT
    SCHEDULED,   // AI/baseline scheduled event
    SYSTEM       // System event (e.g., stop due to error)
};

// Single watering event record
struct WateringEvent {
    time_t startTime;        // Unix timestamp when watering started
    time_t endTime;          // Unix timestamp when watering ended (0 if still running)
    uint8_t zoneId;          // Zone number (1-based)
    uint16_t durationMin;    // Planned duration in minutes
    uint16_t actualDurationSec; // Actual duration in seconds
    EventType eventType;     // Manual, scheduled, or system
    uint32_t scheduleId;     // Schedule ID if scheduled event (0 for manual)
    bool completed;          // True if completed normally, false if interrupted
};

class EventLogger {
public:
    EventLogger();

    // Initialize the logger (call during setup)
    bool begin();

    // Log a new watering event start
    uint32_t logEventStart(uint8_t zoneId, uint16_t durationMin, EventType type, uint32_t scheduleId = 0);

    // Log watering event completion
    bool logEventEnd(uint32_t eventId, bool completed = true);

    // Retrieve events
    String getEventsJson(int limit = 100, time_t startDate = 0, time_t endDate = 0);
    int getEventCount(time_t startDate = 0, time_t endDate = 0);

    // Clear old events (older than specified days)
    int clearOldEvents(int daysToKeep = 365);

    // Clear all events
    bool clearAllEvents();

    // Get statistics
    String getStatistics(time_t startDate = 0, time_t endDate = 0);

private:
    static const char* LOG_FILE;
    static const int MAX_EVENTS_IN_MEMORY = 1000;
    static const int MAX_FILE_SIZE = 512000; // ~500KB max file size

    uint32_t nextEventId;
    WateringEvent currentEvents[4]; // Track up to 4 concurrent events (one per zone)

    // File operations
    bool loadEvents();
    bool saveEvent(const WateringEvent& event);
    bool pruneOldEvents();

    // Helper functions
    String eventToJson(const WateringEvent& event);
    EventType stringToEventType(const String& type);
    String eventTypeToString(EventType type);
    time_t getOneYearAgo();
};

#endif // EVENT_LOGGER_H
