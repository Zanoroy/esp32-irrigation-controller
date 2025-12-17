#include "http_client.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "rtc_module.h"
#include <SPIFFS.h>

HTTPScheduleClient::HTTPScheduleClient() {
    configManager = nullptr;
    scheduleManager = nullptr;
    serverUrl = "http://172.17.254.10:2880";  // Default server
    deviceId = "esp32_irrigation_001";         // Default device ID
    lastFetchTime = 0;
    lastError = "";
    consecutiveFailures = 0;
}

bool HTTPScheduleClient::begin(ConfigManager* config, ScheduleManager* schedule) {
    configManager = config;
    scheduleManager = schedule;

    if (!configManager || !scheduleManager) {
        Serial.println("HTTP Client: Invalid managers provided");
        return false;
    }

    // Initialize SPIFFS for schedule caching
    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        Serial.println("HTTP Client: WARNING - SPIFFS mount failed");
        Serial.println("  Schedule caching will be disabled");
    } else {
        Serial.println("HTTP Client: SPIFFS initialized for caching");
        Serial.println("  Total: " + String(SPIFFS.totalBytes()/1024) + " KB");
        Serial.println("  Used: " + String(SPIFFS.usedBytes()/1024) + " KB");

        // Create /schedules directory if it doesn't exist
        if (!SPIFFS.exists("/schedules")) {
            // SPIFFS doesn't have mkdir, but we can create a dummy file to establish the path
            File dir = SPIFFS.open("/schedules/.init", "w");
            if (dir) {
                dir.close();
                Serial.println("  Created /schedules cache directory");
            }
        }

        // Clean up old cache files (keep last 7 days)
        clearOldCache(7);
    }

    // Get device ID from MAC address if available
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    deviceId = "esp32_" + mac.substring(6);  // Last 6 chars of MAC

    Serial.println("HTTP Schedule Client initialized");
    Serial.println("  Server URL: " + serverUrl);
    Serial.println("  Device ID: " + deviceId);

    return true;
}

void HTTPScheduleClient::setServerUrl(const String& url) {
    serverUrl = url;
    // Remove trailing slash if present
    if (serverUrl.endsWith("/")) {
        serverUrl = serverUrl.substring(0, serverUrl.length() - 1);
    }
    Serial.println("HTTP Client: Server URL set to " + serverUrl);
}

void HTTPScheduleClient::setDeviceId(const String& id) {
    deviceId = id;
    Serial.println("HTTP Client: Device ID set to " + deviceId);
}

String HTTPScheduleClient::buildScheduleUrl(const String& date, int8_t zoneId) {
    String url = serverUrl + "/api/schedules/daily?date=" + date;

    // Add device_id parameter for multi-device filtering
    if (deviceId.length() > 0) {
        url += "&device_id=" + deviceId;
    }

    if (zoneId > 0) {
        url += "&zone_id=" + String(zoneId);
    }
    return url;
}

String HTTPScheduleClient::buildCompletionUrl() {
    return serverUrl + "/api/events/completion";
}

String HTTPScheduleClient::buildEventStartUrl() {
    return serverUrl + "/api/events/start";
}

String HTTPScheduleClient::buildEventSyncUrl() {
    return serverUrl + "/api/events/sync";
}

bool HTTPScheduleClient::executeRequest(const String& url, String& response) {
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        if (retry > 0) {
            Serial.println("HTTP Client: Retry attempt " + String(retry + 1));
            delay(RETRY_DELAY);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("User-Agent", "ESP32-Irrigation/" + deviceId);

        int httpCode = http.GET();

        if (httpCode > 0) {
            response = http.getString();
            http.end();

            if (httpCode == HTTP_CODE_OK) {
                lastError = "";
                consecutiveFailures = 0;  // Reset on success
                return true;
            } else {
                lastError = "HTTP " + String(httpCode) + ": " + response;
                Serial.println("HTTP Client Error: " + lastError);
            }
        } else {
            lastError = "Connection failed: " + http.errorToString(httpCode);
            Serial.println("HTTP Client Error: " + lastError);
        }

        http.end();
    }

    return false;
}

bool HTTPScheduleClient::executePostRequest(const String& url, const String& payload, String& response) {
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        if (retry > 0) {
            Serial.println("HTTP Client: POST retry attempt " + String(retry + 1));
            delay(RETRY_DELAY);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("User-Agent", "ESP32-Irrigation/" + deviceId);

        int httpCode = http.POST(payload);

        if (httpCode > 0) {
            response = http.getString();
            http.end();

            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
                lastError = "";
                return true;
            } else {
                lastError = "HTTP " + String(httpCode) + ": " + response;
                Serial.println("HTTP Client POST Error: " + lastError);
            }
        } else {
            lastError = "POST connection failed: " + http.errorToString(httpCode);
            Serial.println("HTTP Client POST Error: " + lastError);
        }

        http.end();
    }

    return false;
}

bool HTTPScheduleClient::parseScheduleResponse(const String& json, int expectedDays) {
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        lastError = "JSON parse error: " + String(error.c_str());
        Serial.println("HTTP Client: " + lastError);
        return false;
    }

    // Check success field
    bool success = doc["success"] | false;
    if (!success) {
        lastError = doc["error"] | "Unknown error";
        Serial.println("HTTP Client: Server returned error: " + lastError);
        return false;
    }

    // Validate expected days if provided
    if (expectedDays > 0) {
        int daysInResponse = doc["days_returned"] | 1;
        if (daysInResponse != expectedDays) {
            Serial.println("HTTP Client: ⚠️  WARNING - Expected " + String(expectedDays) +
                         " days but received " + String(daysInResponse) + " days");
        }
    }

    // Get data object with date keys
    JsonObject data = doc["data"];
    if (data.isNull()) {
        lastError = "No data object in response";
        Serial.println("HTTP Client: " + lastError);
        Serial.println("HTTP Client: JSON keys found:");
        for (JsonPair kv : doc.as<JsonObject>()) {
            Serial.println("  - " + String(kv.key().c_str()));
        }
        return false;
    }

    int totalEvents = 0;

    // Clear existing server schedules (but keep basic schedules)
    scheduleManager->clearAISchedules();

    Serial.println("HTTP Client: Found " + String(data.size()) + " dates in response");

    // Iterate through each date in the data object
    for (JsonPair datePair : data) {
        String dateStr = datePair.key().c_str();
        JsonVariant dateValue = datePair.value();

        Serial.println("Processing date: " + dateStr);
        Serial.println("  Value type: " + String(dateValue.is<JsonArray>() ? "Array" :
                                                dateValue.is<JsonObject>() ? "Object" : "Other"));

        JsonArray zonesForDate = dateValue.as<JsonArray>();

        if (zonesForDate.isNull()) {
            Serial.println("  ERROR: Could not convert to JsonArray (no zones)");
            continue;
        }

        Serial.println("  Zones count: " + String(zonesForDate.size()));

        // Parse each zone for this date
        for (JsonObject zone : zonesForDate) {
            uint8_t zoneId = zone["zone_id"] | 0;
            String zoneName = zone["zone_name"] | "Unknown";
            JsonArray events = zone["events"];

            Serial.println("  Zone " + String(zoneId) + " (" + zoneName + "): " +
                         String(events.isNull() ? 0 : events.size()) + " events");

            if (events.isNull() || zoneId == 0) {
                if (events.isNull()) Serial.println("    ERROR: events array is null");
                if (zoneId == 0) Serial.println("    ERROR: zoneId is 0");
                continue;
            }

            for (JsonObject event : events) {
                // Parse event data
                uint32_t serverId = event["id"] | 0;
                String startTime = event["start_time"] | "00:00";
                uint16_t durationMin = event["duration_min"] | 0;
                uint8_t repeatCount = event["repeat_count"] | 1;
                uint16_t restTimeMin = event["rest_time_min"] | 0;
                uint8_t priority = event["priority"] | 5;

                Serial.println("  Event: zone=" + String(zoneId) +
                             " time=" + startTime +
                             " duration=" + String(durationMin));

                // Parse start time (format: "HH:MM")
                int colonPos = startTime.indexOf(':');
                if (colonPos <= 0) {
                    Serial.println("  Invalid time format: " + startTime);
                    continue;
                }

                uint8_t hour = startTime.substring(0, colonPos).toInt();
                uint8_t minute = startTime.substring(colonPos + 1).toInt();

                if (hour > 23 || minute > 59) {
                    Serial.println("  Invalid time values: " + String(hour) + ":" + String(minute));
                    continue;
                }

                // Calculate expiry time (end of day in Unix time)
                uint32_t expiryTime = 0;  // 0 means expires end of day (handled by ScheduleManager)

                // Create day mask for today only (will be expanded in Phase 2)
                uint8_t dayMask = 0x7F;  // All days for now (will be refined later)

                // Add to ScheduleManager as AI schedule
                uint8_t scheduleId = scheduleManager->addAISchedule(
                    zoneId, dayMask, hour, minute, durationMin, expiryTime
                );

                if (scheduleId > 0) {
                    totalEvents++;
                    Serial.println("  Added: Zone " + String(zoneId) +
                                 " at " + String(hour) + ":" + String(minute) +
                                 " for " + String(durationMin) + " min (" + dateStr + ")");
                } else {
                    Serial.println("  Failed to add event for zone " + String(zoneId));
                }
            }
        }
    }

    Serial.println("HTTP Client: Successfully loaded " + String(totalEvents) + " events from server");
    
    // Success even if 0 events - empty schedule is valid
    return true;
}

bool HTTPScheduleClient::fetchDailySchedule(const String& date, int8_t zoneId) {
    if (!configManager || !scheduleManager) {
        lastError = "Client not initialized";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        Serial.println("HTTP Client: " + lastError);
        return false;
    }

    Serial.println("HTTP Client: Fetching schedule for " + date +
                   (zoneId > 0 ? " (zone " + String(zoneId) + ")" : " (all zones)"));

    String url = buildScheduleUrl(date, zoneId);
    Serial.println("  URL: " + url);

    String response;
    if (!executeRequest(url, response)) {
        Serial.println("HTTP Client: Request failed - " + lastError);
        return false;
    }

    Serial.println("HTTP Client: Received response (" + String(response.length()) + " bytes)");

    // Parse and load into ScheduleManager
    bool success = parseScheduleResponse(response);

    if (success) {
        lastFetchTime = millis();
    }

    return success;
}

bool HTTPScheduleClient::fetchTodaySchedule() {
    // Get today's date from RTC or system time
    // Format: YYYY-MM-DD
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);

    return fetchDailySchedule(String(dateStr));
}

String HTTPScheduleClient::createCompletionPayload(const EventCompletion& completion) {
    JsonDocument doc;

    doc["schedule_id"] = completion.scheduleId;
    doc["zone_id"] = completion.zoneId;
    doc["device_id"] = completion.deviceId;
    doc["start_time"] = completion.startTime;
    doc["end_time"] = completion.endTime;
    doc["actual_duration_min"] = completion.actualDurationMin;
    doc["water_used_liters"] = completion.waterUsedLiters;
    doc["status"] = completion.status;

    if (completion.notes.length() > 0) {
        doc["notes"] = completion.notes;
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}

bool HTTPScheduleClient::reportCompletion(const EventCompletion& completion) {
    if (!configManager || !scheduleManager) {
        lastError = "Client not initialized";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        Serial.println("HTTP Client: " + lastError + " - saving event for later sync");

        // Save to pending events queue for sync when online
        if (savePendingEvent(completion)) {
            Serial.println("HTTP Client: ✅ Event saved to pending queue");
            return true;  // Return true since we successfully queued it
        } else {
            Serial.println("HTTP Client: ❌ Failed to save pending event");
            return false;
        }
    }

    Serial.println("HTTP Client: Reporting completion for schedule " + String(completion.scheduleId));

    String url = buildCompletionUrl();
    String payload = createCompletionPayload(completion);

    Serial.println("  URL: " + url);
    Serial.println("  Payload: " + payload);

    String response;
    bool success = executePostRequest(url, payload, response);

    if (success) {
        Serial.println("HTTP Client: Completion reported successfully");
        Serial.println("  Response: " + response);
    } else {
        Serial.println("HTTP Client: Failed to report completion - " + lastError);
        Serial.println("HTTP Client: Saving event for later sync");

        // If POST failed, save to pending queue
        savePendingEvent(completion);
    }

    return success;
}

bool HTTPScheduleClient::reportCompletion(uint32_t scheduleId, uint8_t zoneId,
                                         float durationMin, float waterUsed,
                                         const String& status) {
    // Get current time for timestamps
    time_t now = time(nullptr);
    time_t startTime = now - (uint32_t)(durationMin * 60);

    struct tm timeinfo;
    char startTimeStr[25];
    char endTimeStr[25];

    localtime_r(&startTime, &timeinfo);
    strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    localtime_r(&now, &timeinfo);
    strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    EventCompletion completion;
    completion.scheduleId = scheduleId;
    completion.zoneId = zoneId;
    completion.deviceId = deviceId;
    completion.startTime = String(startTimeStr);
    completion.endTime = String(endTimeStr);
    completion.actualDurationMin = durationMin;
    completion.waterUsedLiters = waterUsed;
    completion.status = status;
    completion.notes = "";

    return reportCompletion(completion);
}

bool HTTPScheduleClient::testConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        Serial.println("HTTP Client Test: " + lastError);
        return false;
    }

    Serial.println("HTTP Client: Testing connection to " + serverUrl);

    String url = serverUrl + "/api/system-settings";  // Use a simple endpoint
    String response;

    bool success = executeRequest(url, response);

    if (success) {
        Serial.println("HTTP Client Test: Connection successful");
    } else {
        Serial.println("HTTP Client Test: Connection failed - " + lastError);
    }

    return success;
}

// ===== NEW METHODS FOR 5-DAY LOOKAHEAD AND EVENT TRACKING =====

bool HTTPScheduleClient::fetch5DaySchedule(int8_t zoneId) {
    // This is now just a wrapper around the unified fetchSchedule method
    return fetchSchedule(5, zoneId);
}

bool HTTPScheduleClient::parse5DayScheduleResponse(const String& json) {
    // Parse JSON response for 5-day schedule
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        lastError = "JSON parse error: " + String(error.c_str());
        Serial.println("HTTP Client: " + lastError);
        return false;
    }

    // Check success field
    bool success = doc["success"] | false;
    if (!success) {
        lastError = doc["error"] | "Unknown error";
        Serial.println("HTTP Client: Server returned error: " + lastError);
        return false;
    }

    // Get schedule array (organized by date)
    JsonObject data = doc["data"];
    if (data.isNull()) {
        lastError = "No data object in response";
        Serial.println("HTTP Client: " + lastError);
        return false;
    }

    int totalEvents = 0;
    int daysProcessed = 0;

    // Clear existing AI schedules before loading new ones
    scheduleManager->clearAISchedules();

    // Iterate through each day in the response
    for (JsonPair dayPair : data) {
        String date = dayPair.key().c_str();
        JsonArray zonesForDay = dayPair.value();

        Serial.println("Processing date: " + date + " (" + String(zonesForDay.size()) + " zones)");
        daysProcessed++;

        // TODO: Save to SPIFFS as /schedule_YYYY-MM-DD.json for offline resilience
        // This will be implemented in Phase 2 with proper SPIFFS caching

        // Each zone has an events array
        for (JsonObject zone : zonesForDay) {
            uint8_t zoneId = zone["zone_id"] | 0;
            JsonArray events = zone["events"];

            if (zoneId == 0 || events.isNull()) continue;

            Serial.println("  Zone " + String(zoneId) + ": " + String(events.size()) + " events");

            // Process each event for this zone
            for (JsonObject event : events) {
                uint32_t serverId = event["id"] | 0;
                String startTime = event["start_time"] | "00:00";
                uint16_t durationMin = event["duration_min"] | 0;
                uint8_t repeatCount = event["repeat_count"] | 1;
                uint16_t restTimeMin = event["rest_time_min"] | 0;

                Serial.println("    Event: id=" + String(serverId) + " time=" + startTime + " duration=" + String(durationMin));

                // Parse start time (format: "HH:MM")
                int colonPos = startTime.indexOf(':');
                if (colonPos <= 0) continue;

                uint8_t hour = startTime.substring(0, colonPos).toInt();
                uint8_t minute = startTime.substring(colonPos + 1).toInt();

                if (hour > 23 || minute > 59) continue;

                // Add to ScheduleManager (AI schedule with expiry at end of day)
                uint8_t scheduleId = scheduleManager->addAISchedule(
                    zoneId, 0x7F, hour, minute, durationMin, 0
                );

                if (scheduleId > 0) {
                    totalEvents++;
                    Serial.println("  Added: Zone " + String(zoneId) + " at " +
                                String(hour) + ":" + String(minute) + " (" + date + ")");
                }
            }
        }
    }
    Serial.println("HTTP Client: Loaded " + String(totalEvents) +
                   " events across " + String(daysProcessed) + " days");

    return (totalEvents > 0);
}

String HTTPScheduleClient::createEventStartPayload(uint32_t scheduleId, uint8_t zoneId, const String& startTime) {
    JsonDocument doc;

    doc["schedule_id"] = scheduleId;
    doc["zone_id"] = zoneId;
    doc["device_id"] = deviceId;
    doc["start_time"] = startTime;
    doc["status"] = "running";

    String payload;
    serializeJson(doc, payload);
    return payload;
}

bool HTTPScheduleClient::reportEventStart(uint32_t scheduleId, uint8_t zoneId, const String& startTime) {
    if (!configManager || !scheduleManager) {
        lastError = "Client not initialized";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        Serial.println("HTTP Client: " + lastError + " - event start will be buffered");
        // TODO: Buffer this event for later submission when connection restored (Phase 2)
        return false;
    }

    Serial.println("HTTP Client: Reporting event start for schedule " + String(scheduleId));

    String url = buildEventStartUrl();
    String payload = createEventStartPayload(scheduleId, zoneId, startTime);

    Serial.println("  URL: " + url);

    String response;
    bool success = executePostRequest(url, payload, response);

    if (success) {
        Serial.println("HTTP Client: Event start reported successfully");
    } else {
        Serial.println("HTTP Client: Failed to report event start - " + lastError);
        // TODO: Buffer for retry (Phase 2)
    }

    return success;
}

// ===== SPIFFS CACHING METHODS =====

bool HTTPScheduleClient::cacheScheduleToSPIFFS(const String& date, const String& json) {
    if (!SPIFFS.begin()) {
        Serial.println("HTTP Client: SPIFFS not available for caching");
        return false;
    }

    String filepath = "/schedules/" + date + ".json";

    File file = SPIFFS.open(filepath, "w");
    if (!file) {
        Serial.println("HTTP Client: Failed to create cache file: " + filepath);
        return false;
    }

    size_t bytesWritten = file.print(json);
    file.close();

    if (bytesWritten > 0) {
        Serial.println("HTTP Client: Cached schedule to " + filepath + " (" + String(bytesWritten) + " bytes)");
        return true;
    } else {
        Serial.println("HTTP Client: Failed to write cache file");
        return false;
    }
}

bool HTTPScheduleClient::loadScheduleFromCache(const String& date) {
    if (!SPIFFS.begin()) {
        Serial.println("HTTP Client: SPIFFS not available");
        return false;
    }

    String filepath = "/schedules/" + date + ".json";

    if (!SPIFFS.exists(filepath)) {
        Serial.println("HTTP Client: No cached schedule found for " + date);
        return false;
    }

    File file = SPIFFS.open(filepath, "r");
    if (!file) {
        Serial.println("HTTP Client: Failed to open cache file: " + filepath);
        return false;
    }

    String json = file.readString();
    file.close();

    Serial.println("HTTP Client: Loading cached schedule from " + filepath + " (" + String(json.length()) + " bytes)");

    // Parse and load the cached schedule
    bool success = parse5DayScheduleResponse(json);

    if (success) {
        Serial.println("HTTP Client: ✅ Successfully loaded schedule from cache");
    } else {
        Serial.println("HTTP Client: ❌ Failed to parse cached schedule");
    }

    return success;
}

bool HTTPScheduleClient::loadLatestCachedSchedule() {
    if (!SPIFFS.begin()) {
        Serial.println("HTTP Client: SPIFFS not available");
        return false;
    }

    // Get today's date
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);

    // Try to load today's cache first
    if (loadScheduleFromCache(String(dateStr))) {
        return true;
    }

    // If today's cache doesn't exist, try yesterday's
    now -= 86400;  // Subtract 1 day
    localtime_r(&now, &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);

    if (loadScheduleFromCache(String(dateStr))) {
        Serial.println("HTTP Client: Using yesterday's cached schedule as fallback");
        return true;
    }

    Serial.println("HTTP Client: No recent cached schedules found");
    return false;
}

bool HTTPScheduleClient::clearOldCache(int daysToKeep) {
    if (!SPIFFS.begin()) {
        return false;
    }

    time_t now = time(nullptr);
    time_t cutoffTime = now - (daysToKeep * 86400);  // Convert days to seconds

    File root = SPIFFS.open("/schedules");
    if (!root || !root.isDirectory()) {
        Serial.println("HTTP Client: /schedules directory not found");
        return false;
    }

    int filesDeleted = 0;
    File file = root.openNextFile();

    while (file) {
        String filename = String(file.name());

        // Only process .json files in the schedules directory
        if (filename.endsWith(".json") && !filename.equals(".init")) {
            time_t fileTime = file.getLastWrite();

            if (fileTime < cutoffTime) {
                String filepath = "/schedules/" + filename;
                file.close();

                if (SPIFFS.remove(filepath)) {
                    Serial.println("HTTP Client: Deleted old cache: " + filepath);
                    filesDeleted++;
                }
            }
        }

        file = root.openNextFile();
    }

    root.close();

    if (filesDeleted > 0) {
        Serial.println("HTTP Client: Cleaned up " + String(filesDeleted) + " old cache files");
    }

    return true;
}

// Unified fetch method that supports 1-5 days by calling API once per day
bool HTTPScheduleClient::fetchSchedule(int days, int8_t zoneId) {
    if (days < 1 || days > 5) {
        lastError = "Invalid days parameter (must be 1-5)";
        return false;
    }

    if (!configManager || !scheduleManager) {
        lastError = "Client not initialized";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        Serial.println("HTTP Client: " + lastError + " - attempting to load from cache");
        consecutiveFailures++;

        // Try to load from cache as fallback
        return loadLatestCachedSchedule();
    }

    Serial.println("HTTP Client: Fetching " + String(days) + "-day schedule" +
                   (zoneId > 0 ? " (zone " + String(zoneId) + ")" : " (all zones)"));
    Serial.println("  Server: " + serverUrl);

    // Clear existing AI schedules before loading new ones
    scheduleManager->clearAISchedules();

    // Get current time for calculating dates
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int totalEventsLoaded = 0;
    int daysSuccessful = 0;

    // Fetch schedule for each day
    for (int dayOffset = 0; dayOffset < days; dayOffset++) {
        // Calculate date for this iteration
        time_t targetTime = now + (dayOffset * 86400);  // Add days in seconds
        localtime_r(&targetTime, &timeinfo);

        char dateStr[11];
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);

        Serial.println("\n  Day " + String(dayOffset + 1) + "/" + String(days) + ": " + String(dateStr));

        String url = buildScheduleUrl(String(dateStr), zoneId);
        Serial.println("    URL: " + url);

        String response;
        if (!executeRequest(url, response)) {
            Serial.println("    ⚠️  Failed to fetch - " + lastError);
            continue;  // Continue with next day even if this one fails
        }

        Serial.println("    Received (" + String(response.length()) + " bytes)");

        // Parse this day's schedule and add to ScheduleManager
        if (parseScheduleResponse(response, 1)) {
            daysSuccessful++;
            Serial.println("    ✅ Loaded schedules successfully");

            // Cache this day's response
            cacheScheduleToSPIFFS(String(dateStr), response);
        } else {
            Serial.println("    ⚠️  Failed to parse response");
        }

        // Small delay between requests to avoid overwhelming server
        if (dayOffset < days - 1) {
            delay(100);
        }
    }

    Serial.println("");
    if (daysSuccessful > 0) {
        lastFetchTime = millis();
        consecutiveFailures = 0;
        Serial.println("HTTP Client: ✅ Successfully loaded schedules from " +
                       String(daysSuccessful) + "/" + String(days) + " days");

        // After successful schedule fetch, sync any pending events
        if (getPendingEventCount() > 0) {
            Serial.println("HTTP Client: 📤 Syncing " + String(getPendingEventCount()) + " pending events...");
            syncPendingEvents();
        }

        return true;
    } else {
        consecutiveFailures++;
        Serial.println("HTTP Client: ❌ Failed to fetch any schedules");
        Serial.println("  Attempting to load from cache...");
        return loadLatestCachedSchedule();
    }
}

/**
 * Save event to pending queue (for offline sync)
 * Events are stored in SPIFFS and synced when connection restored
 */
bool HTTPScheduleClient::savePendingEvent(const EventCompletion& completion) {
    if (!SPIFFS.begin()) {
        Serial.println("HTTP Client: Failed to mount SPIFFS for pending event save");
        return false;
    }

    // Create /events directory if it doesn't exist
    if (!SPIFFS.exists("/events")) {
        File dir = SPIFFS.open("/events/.init", "w");
        if (dir) {
            dir.close();
        }
    }

    // Generate filename: /events/pending_{timestamp}_{zoneId}.json
    String filename = "/events/pending_" + String(millis()) + "_z" + String(completion.zoneId) + ".json";

    File file = SPIFFS.open(filename, "w");
    if (!file) {
        Serial.println("HTTP Client: Failed to create pending event file: " + filename);
        return false;
    }

    // Write event data as JSON
    JsonDocument doc;
    doc["schedule_id"] = completion.scheduleId;
    doc["zone_id"] = completion.zoneId;
    doc["device_id"] = completion.deviceId;
    doc["start_time"] = completion.startTime;
    doc["end_time"] = completion.endTime;
    doc["duration_min"] = completion.actualDurationMin;
    doc["water_used_liters"] = completion.waterUsedLiters;
    doc["status"] = completion.status;
    doc["notes"] = completion.notes;
    doc["saved_at"] = String(millis());

    serializeJson(doc, file);
    file.close();

    Serial.println("HTTP Client: 💾 Saved pending event: " + filename);
    return true;
}

/**
 * Get count of pending events waiting to be synced
 */
int HTTPScheduleClient::getPendingEventCount() {
    if (!SPIFFS.begin()) {
        return 0;
    }

    int count = 0;
    File root = SPIFFS.open("/events");
    if (!root || !root.isDirectory()) {
        return 0;
    }

    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        if (name.startsWith("/events/pending_")) {
            count++;
        }
        file = root.openNextFile();
    }

    return count;
}

/**
 * Sync all pending events to server
 * Called after successful schedule fetch or WiFi reconnection
 */
bool HTTPScheduleClient::syncPendingEvents() {
    if (!SPIFFS.begin()) {
        Serial.println("HTTP Client: Failed to mount SPIFFS for event sync");
        return false;
    }

    // Collect all pending events
    JsonDocument eventsDoc;
    JsonArray events = eventsDoc["events"].to<JsonArray>();
    eventsDoc["device_id"] = deviceId;

    File root = SPIFFS.open("/events");
    if (!root || !root.isDirectory()) {
        Serial.println("HTTP Client: No pending events to sync");
        return true;
    }

    int eventCount = 0;
    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        if (name.startsWith("/events/pending_")) {
            // Parse event file
            JsonDocument eventDoc;
            DeserializationError error = deserializeJson(eventDoc, file);

            if (!error) {
                JsonObject event = events.add<JsonObject>();
                event["schedule_id"] = eventDoc["schedule_id"];
                event["zone_id"] = eventDoc["zone_id"];
                event["start_time"] = eventDoc["start_time"];
                event["end_time"] = eventDoc["end_time"];
                event["duration_min"] = eventDoc["duration_min"];
                event["water_used_liters"] = eventDoc["water_used_liters"];
                event["completed"] = (eventDoc["status"].as<String>() == "completed");
                event["status"] = eventDoc["status"];
                event["notes"] = eventDoc["notes"];

                eventCount++;
            } else {
                Serial.println("HTTP Client: Failed to parse event file: " + name);
            }
        }
        file.close();
        file = root.openNextFile();
    }

    if (eventCount == 0) {
        Serial.println("HTTP Client: No valid pending events to sync");
        return true;
    }

    // Serialize payload
    String payload;
    serializeJson(eventsDoc, payload);

    Serial.println("HTTP Client: 📤 Syncing " + String(eventCount) + " pending events");
    Serial.println("  Payload size: " + String(payload.length()) + " bytes");

    // POST to /api/events/sync
    String url = buildEventSyncUrl();
    String response;

    if (!executePostRequest(url, payload, response)) {
        Serial.println("HTTP Client: ❌ Failed to sync events - " + lastError);
        return false;
    }

    // Parse response
    JsonDocument respDoc;
    DeserializationError error = deserializeJson(respDoc, response);

    if (error) {
        Serial.println("HTTP Client: Failed to parse sync response");
        return false;
    }

    bool success = respDoc["success"];
    int synced = respDoc["synced"] | 0;
    int skipped = respDoc["skipped"] | 0;
    int errors = respDoc["errors"] | 0;

    if (success) {
        Serial.println("HTTP Client: ✅ Event sync completed");
        Serial.println("  Synced: " + String(synced) + ", Skipped: " + String(skipped) + ", Errors: " + String(errors));

        // Delete successfully synced event files
        File root2 = SPIFFS.open("/events");
        File file2 = root2.openNextFile();
        while (file2) {
            String name = file2.name();
            if (name.startsWith("/events/pending_")) {
                file2.close();
                SPIFFS.remove(name);
                Serial.println("  Removed: " + name);
                file2 = root2.openNextFile();
            } else {
                file2.close();
                file2 = root2.openNextFile();
            }
        }

        return true;
    } else {
        Serial.println("HTTP Client: ⚠️  Event sync partially failed");
        return false;
    }
}
