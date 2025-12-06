#include "event_logger.h"
#include <RTClib.h>

const char* EventLogger::LOG_FILE = "/events.jsonl";

EventLogger::EventLogger() : nextEventId(1) {
    // Initialize current events array
    for (int i = 0; i < 4; i++) {
        currentEvents[i].startTime = 0;
        currentEvents[i].endTime = 0;
        currentEvents[i].zoneId = 0;
    }
}

bool EventLogger::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("EventLogger: Failed to mount SPIFFS");
        return false;
    }

    // Check if log file exists, create if not
    if (!SPIFFS.exists(LOG_FILE)) {
        File file = SPIFFS.open(LOG_FILE, FILE_WRITE);
        if (!file) {
            Serial.println("EventLogger: Failed to create log file");
            return false;
        }
        file.close();
        Serial.println("EventLogger: Created new log file");
    }

    // Load next event ID from existing logs
    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (file) {
        uint32_t maxId = 0;
        while (file.available()) {
            String line = file.readStringUntil('\n');
            JsonDocument doc;
            if (deserializeJson(doc, line) == DeserializationError::Ok) {
                uint32_t id = doc["id"] | 0;
                if (id > maxId) maxId = id;
            }
        }
        file.close();
        nextEventId = maxId + 1;
    }

    // Prune old events on startup
    pruneOldEvents();

    Serial.println("EventLogger: Initialized (next ID: " + String(nextEventId) + ")");
    return true;
}

uint32_t EventLogger::logEventStart(uint8_t zoneId, uint16_t durationMin, EventType type, uint32_t scheduleId) {
    if (zoneId < 1 || zoneId > 4) {
        Serial.println("EventLogger: Invalid zone ID: " + String(zoneId));
        return 0;
    }

    time_t now = time(nullptr);
    if (now < 1000000000) { // Sanity check for valid timestamp
        Serial.println("EventLogger: Invalid system time, cannot log event");
        return 0;
    }

    uint32_t eventId = nextEventId++;
    int idx = zoneId - 1;

    // Store in current events
    currentEvents[idx].startTime = now;
    currentEvents[idx].endTime = 0;
    currentEvents[idx].zoneId = zoneId;
    currentEvents[idx].durationMin = durationMin;
    currentEvents[idx].actualDurationSec = 0;
    currentEvents[idx].eventType = type;
    currentEvents[idx].scheduleId = scheduleId;
    currentEvents[idx].completed = false;

    // Log immediately to file (start event)
    JsonDocument doc;
    doc["id"] = eventId;
    doc["zone_id"] = zoneId;
    doc["start_time"] = now;
    doc["duration_min"] = durationMin;
    doc["actual_duration_sec"] = 0;
    doc["type"] = eventTypeToString(type);
    if (scheduleId > 0) {
        doc["schedule_id"] = scheduleId;
    }
    doc["completed"] = false;
    doc["status"] = "running";

    String jsonLine;
    serializeJson(doc, jsonLine);

    File file = SPIFFS.open(LOG_FILE, FILE_APPEND);
    if (file) {
        file.println(jsonLine);
        file.close();
        Serial.println("EventLogger: Started event " + String(eventId) +
                      " (Zone " + String(zoneId) + ", " +
                      String(durationMin) + " min, " +
                      eventTypeToString(type) + ")");
        return eventId;
    } else {
        Serial.println("EventLogger: Failed to write start event");
        return 0;
    }
}

bool EventLogger::logEventEnd(uint32_t eventId, bool completed) {
    time_t now = time(nullptr);
    if (now < 1000000000) {
        Serial.println("EventLogger: Invalid system time, cannot log event end");
        return false;
    }

    // Find the event in current events
    WateringEvent* event = nullptr;
    for (int i = 0; i < 4; i++) {
        if (currentEvents[i].zoneId > 0 && currentEvents[i].startTime > 0) {
            // Match by zone and start time (approximate event ID)
            event = &currentEvents[i];
            break;
        }
    }

    if (!event) {
        Serial.println("EventLogger: Event not found in current events");
        return false;
    }

    event->endTime = now;
    event->actualDurationSec = (uint16_t)(now - event->startTime);
    event->completed = completed;

    // Log completion to file
    JsonDocument doc;
    doc["id"] = eventId;
    doc["zone_id"] = event->zoneId;
    doc["start_time"] = event->startTime;
    doc["end_time"] = now;
    doc["duration_min"] = event->durationMin;
    doc["actual_duration_sec"] = event->actualDurationSec;
    doc["type"] = eventTypeToString(event->eventType);
    if (event->scheduleId > 0) {
        doc["schedule_id"] = event->scheduleId;
    }
    doc["completed"] = completed;
    doc["status"] = completed ? "completed" : "interrupted";

    String jsonLine;
    serializeJson(doc, jsonLine);

    File file = SPIFFS.open(LOG_FILE, FILE_APPEND);
    if (file) {
        file.println(jsonLine);
        file.close();

        Serial.println("EventLogger: Ended event " + String(eventId) +
                      " (Zone " + String(event->zoneId) + ", " +
                      String(event->actualDurationSec) + " sec, " +
                      (completed ? "completed" : "interrupted") + ")");

        // Clear from current events
        event->zoneId = 0;
        event->startTime = 0;

        return true;
    } else {
        Serial.println("EventLogger: Failed to write end event");
        return false;
    }
}

String EventLogger::getEventsJson(int limit, time_t startDate, time_t endDate) {
    JsonDocument doc;
    JsonArray events = doc["events"].to<JsonArray>();

    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) {
        doc["error"] = "Failed to open log file";
        String output;
        serializeJson(doc, output);
        return output;
    }

    int count = 0;
    int total = 0;

    // Read all events (JSONL format - one JSON object per line)
    while (file.available() && count < limit) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument eventDoc;
        DeserializationError error = deserializeJson(eventDoc, line);

        if (error) continue;

        // Filter by date range if specified
        time_t eventTime = eventDoc["start_time"] | 0;
        if (startDate > 0 && eventTime < startDate) continue;
        if (endDate > 0 && eventTime > endDate) continue;

        // Only include completed events (those with end_time)
        if (!eventDoc["end_time"].isNull()) {
            events.add(eventDoc.as<JsonObject>());
            count++;
        }
        total++;
    }

    file.close();

    doc["count"] = count;
    doc["total"] = total;
    doc["limit"] = limit;

    String output;
    serializeJson(doc, output);
    return output;
}

int EventLogger::getEventCount(time_t startDate, time_t endDate) {
    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) return 0;

    int count = 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument doc;
        if (deserializeJson(doc, line) != DeserializationError::Ok) continue;

        // Only count completed events
        if (doc["end_time"].isNull()) continue;

        time_t eventTime = doc["start_time"] | 0;
        if (startDate > 0 && eventTime < startDate) continue;
        if (endDate > 0 && eventTime > endDate) continue;

        count++;
    }

    file.close();
    return count;
}

int EventLogger::clearOldEvents(int daysToKeep) {
    time_t cutoffTime = time(nullptr) - (daysToKeep * 24 * 60 * 60);

    File readFile = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!readFile) {
        Serial.println("EventLogger: Failed to open log file for reading");
        return 0;
    }

    File writeFile = SPIFFS.open("/events_temp.jsonl", FILE_WRITE);
    if (!writeFile) {
        readFile.close();
        Serial.println("EventLogger: Failed to create temp file");
        return 0;
    }

    int removed = 0;
    int kept = 0;

    while (readFile.available()) {
        String line = readFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument doc;
        if (deserializeJson(doc, line) != DeserializationError::Ok) {
            writeFile.println(line); // Keep malformed lines
            continue;
        }

        time_t eventTime = doc["start_time"] | 0;

        if (eventTime >= cutoffTime) {
            writeFile.println(line);
            kept++;
        } else {
            removed++;
        }
    }

    readFile.close();
    writeFile.close();

    // Replace old file with new file
    SPIFFS.remove(LOG_FILE);
    SPIFFS.rename("/events_temp.jsonl", LOG_FILE);

    Serial.println("EventLogger: Cleared " + String(removed) + " old events, kept " + String(kept));
    return removed;
}

bool EventLogger::clearAllEvents() {
    if (SPIFFS.remove(LOG_FILE)) {
        // Recreate empty file
        File file = SPIFFS.open(LOG_FILE, FILE_WRITE);
        if (file) {
            file.close();
            nextEventId = 1;
            Serial.println("EventLogger: Cleared all events");
            return true;
        }
    }
    Serial.println("EventLogger: Failed to clear events");
    return false;
}

String EventLogger::getStatistics(time_t startDate, time_t endDate) {
    JsonDocument doc;

    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) {
        doc["error"] = "Failed to open log file";
        String output;
        serializeJson(doc, output);
        return output;
    }

    int totalEvents = 0;
    int completedEvents = 0;
    int interruptedEvents = 0;
    uint32_t totalWateringSeconds = 0;
    int zoneCount[4] = {0, 0, 0, 0};
    int manualEvents = 0;
    int scheduledEvents = 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument eventDoc;
        if (deserializeJson(eventDoc, line) != DeserializationError::Ok) continue;

        // Only process completed events
        if (eventDoc["end_time"].isNull()) continue;

        time_t eventTime = eventDoc["start_time"] | 0;
        if (startDate > 0 && eventTime < startDate) continue;
        if (endDate > 0 && eventTime > endDate) continue;

        totalEvents++;

        bool completed = eventDoc["completed"] | false;
        if (completed) {
            completedEvents++;
        } else {
            interruptedEvents++;
        }

        uint32_t duration = eventDoc["actual_duration_sec"] | 0;
        totalWateringSeconds += duration;

        uint8_t zone = eventDoc["zone_id"] | 0;
        if (zone >= 1 && zone <= 4) {
            zoneCount[zone - 1]++;
        }

        String type = eventDoc["type"] | "";
        if (type == "manual") {
            manualEvents++;
        } else if (type == "scheduled") {
            scheduledEvents++;
        }
    }

    file.close();

    doc["total_events"] = totalEvents;
    doc["completed_events"] = completedEvents;
    doc["interrupted_events"] = interruptedEvents;
    doc["total_watering_seconds"] = totalWateringSeconds;
    doc["total_watering_hours"] = (float)totalWateringSeconds / 3600.0;
    doc["manual_events"] = manualEvents;
    doc["scheduled_events"] = scheduledEvents;

    JsonArray zones = doc["events_per_zone"].to<JsonArray>();
    for (int i = 0; i < 4; i++) {
        JsonObject zoneObj = zones.add<JsonObject>();
        zoneObj["zone_id"] = i + 1;
        zoneObj["count"] = zoneCount[i];
    }

    String output;
    serializeJson(doc, output);
    return output;
}

bool EventLogger::pruneOldEvents() {
    time_t oneYearAgo = getOneYearAgo();

    // Check file size
    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) return false;

    size_t fileSize = file.size();
    file.close();

    // If file is too large or we need to prune old events
    if (fileSize > MAX_FILE_SIZE) {
        Serial.println("EventLogger: File size " + String(fileSize) + " exceeds max, pruning...");
        clearOldEvents(365);
        return true;
    }

    return false;
}

String EventLogger::eventTypeToString(EventType type) {
    switch (type) {
        case EventType::MANUAL: return "manual";
        case EventType::SCHEDULED: return "scheduled";
        case EventType::SYSTEM: return "system";
        default: return "unknown";
    }
}

time_t EventLogger::getOneYearAgo() {
    time_t now = time(nullptr);
    return now - (365 * 24 * 60 * 60);
}
