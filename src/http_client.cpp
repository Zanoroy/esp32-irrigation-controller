#include "http_client.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "rtc_module.h"

HTTPScheduleClient::HTTPScheduleClient() {
    configManager = nullptr;
    scheduleManager = nullptr;
    serverUrl = "http://172.17.254.10:2880";  // Default server
    deviceId = "esp32_irrigation_001";         // Default device ID
    lastFetchTime = 0;
    lastError = "";
}

bool HTTPScheduleClient::begin(ConfigManager* config, ScheduleManager* schedule) {
    configManager = config;
    scheduleManager = schedule;
    
    if (!configManager || !scheduleManager) {
        Serial.println("HTTP Client: Invalid managers provided");
        return false;
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
    if (zoneId > 0) {
        url += "&zone_id=" + String(zoneId);
    }
    return url;
}

String HTTPScheduleClient::buildCompletionUrl() {
    return serverUrl + "/api/events/completion";
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

bool HTTPScheduleClient::parseScheduleResponse(const String& json) {
    // Parse JSON response
    DynamicJsonDocument doc(8192);  // 8KB buffer for schedule JSON
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
    
    // Get zones array
    JsonArray zones = doc["zones"];
    if (zones.isNull()) {
        lastError = "No zones array in response";
        Serial.println("HTTP Client: " + lastError);
        return false;
    }
    
    int totalEvents = 0;
    
    // Clear existing server schedules (but keep basic schedules)
    // Note: This requires adding clearAISchedules() method to ScheduleManager
    scheduleManager->clearAISchedules();
    
    // Parse each zone's events
    for (JsonObject zone : zones) {
        uint8_t zoneId = zone["zone_id"] | 0;
        String zoneName = zone["zone_name"] | "Unknown";
        JsonArray events = zone["events"];
        
        if (events.isNull() || zoneId == 0) {
            continue;
        }
        
        Serial.println("Processing zone " + String(zoneId) + " (" + zoneName + ")");
        
        for (JsonObject event : events) {
            // Parse event data
            uint32_t serverId = event["id"] | 0;
            String startTime = event["start_time"] | "00:00";
            uint16_t durationMin = event["duration_min"] | 0;
            uint8_t repeatCount = event["repeat_count"] | 1;
            uint16_t restTimeMin = event["rest_time_min"] | 0;
            uint8_t priority = event["priority"] | 5;
            
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
            // For now, set to expire at midnight (24 hours from schedule generation)
            uint32_t expiryTime = 0;  // 0 means expires end of day (handled by ScheduleManager)
            
            // Create day mask for today only (will be expanded in Phase 2)
            uint8_t dayMask = 0x7F;  // All days for now (will be refined later)
            
            // Add to ScheduleManager as AI schedule
            uint8_t scheduleId = scheduleManager->addAISchedule(
                zoneId, dayMask, hour, minute, durationMin, expiryTime
            );
            
            if (scheduleId > 0) {
                totalEvents++;
                Serial.println("  Added event: Zone " + String(zoneId) + 
                             " at " + String(hour) + ":" + String(minute) + 
                             " for " + String(durationMin) + " min (ID: " + String(scheduleId) + ")");
            } else {
                Serial.println("  Failed to add event for zone " + String(zoneId));
            }
        }
    }
    
    Serial.println("HTTP Client: Successfully loaded " + String(totalEvents) + " events from server");
    return totalEvents > 0;
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
    DynamicJsonDocument doc(1024);
    
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
        Serial.println("HTTP Client: " + lastError + " - completion report queued for later");
        // TODO: Add to retry queue (Phase 2)
        return false;
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
        // TODO: Add to retry queue (Phase 2)
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
