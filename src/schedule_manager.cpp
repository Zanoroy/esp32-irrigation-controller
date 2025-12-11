#include "schedule_manager.h"
#include "config_manager.h"
#include "rtc_module.h"
#include <ArduinoJson.h>

ScheduleManager::ScheduleManager() {
    scheduleCount = 0;
    nextScheduleId = 1;
    configManager = nullptr;
    rtcModule = nullptr;

    // Initialize rain control
    rainDelayActive = false;
    rainDelayEndTime = 0;
    scheduleEnabled = true;

    // Clear all schedules
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i].id = 0;
        schedules[i].enabled = false;
    }

    // Clear active zones
    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        activeZones[i].zone = 0;
        activeZones[i].state = IDLE;
        activeZones[i].startTime = 0;
        activeZones[i].duration = 0;
        activeZones[i].isScheduled = false;
        activeZones[i].scheduleId = 0;
        activeZones[i].timeRemaining = 0;
    }
}

bool ScheduleManager::begin(ConfigManager* config, RTCModule* rtc) {
    configManager = config;
    rtcModule = rtc;
    Serial.println("ScheduleManager: Initialized");
    return true;
}

uint8_t ScheduleManager::addBasicSchedule(uint8_t zone, uint8_t dayMask, uint8_t hour, uint8_t minute, uint16_t duration) {
    if (!configManager || !configManager->isZoneEnabled(zone)) {
        Serial.printf("ScheduleManager: Zone %d not enabled\n", zone);
        return 0;
    }

    uint8_t slot = findFreeScheduleSlot();
    if (slot >= MAX_SCHEDULES) {
        Serial.println("ScheduleManager: No free schedule slots");
        return 0;
    }

    schedules[slot].id = nextScheduleId++;
    schedules[slot].zone = zone;
    schedules[slot].dayMask = dayMask;
    schedules[slot].startHour = hour;
    schedules[slot].startMinute = minute;
    schedules[slot].duration = duration;
    schedules[slot].enabled = true;
    schedules[slot].type = BASIC;
    schedules[slot].createdTime = getCurrentUnixTime();
    schedules[slot].expiryTime = 0; // Never expires

    scheduleCount++;
    Serial.printf("ScheduleManager: Added basic schedule ID %d for zone %d\n", schedules[slot].id, zone);
    return schedules[slot].id;
}

uint8_t ScheduleManager::addAISchedule(uint8_t zone, uint8_t dayMask, uint8_t hour, uint8_t minute, uint16_t duration, uint32_t expiryTime) {
    if (!configManager || !configManager->isZoneEnabled(zone)) {
        Serial.printf("ScheduleManager: Zone %d not enabled\n", zone);
        return 0;
    }

    uint8_t slot = findFreeScheduleSlot();
    if (slot >= MAX_SCHEDULES) {
        Serial.println("ScheduleManager: No free schedule slots");
        return 0;
    }

    schedules[slot].id = nextScheduleId++;
    schedules[slot].zone = zone;
    schedules[slot].dayMask = dayMask;
    schedules[slot].startHour = hour;
    schedules[slot].startMinute = minute;
    schedules[slot].duration = duration;
    schedules[slot].enabled = true;
    schedules[slot].type = AI;
    schedules[slot].createdTime = getCurrentUnixTime();
    schedules[slot].expiryTime = expiryTime;

    scheduleCount++;
    Serial.printf("ScheduleManager: Added AI schedule ID %d for zone %d (expires in %d hours)\n",
                  schedules[slot].id, zone, (expiryTime - getCurrentUnixTime()) / 3600);
    return schedules[slot].id;
}

bool ScheduleManager::removeSchedule(uint8_t id) {
    uint8_t slot = findScheduleById(id);
    if (slot >= MAX_SCHEDULES) {
        return false;
    }

    schedules[slot].id = 0;
    schedules[slot].enabled = false;
    scheduleCount--;

    Serial.printf("ScheduleManager: Removed schedule ID %d\n", id);
    return true;
}

void ScheduleManager::checkAndExecuteSchedules() {
    if (!configManager) return;

    // Clean up expired AI schedules first
    cleanupExpiredAISchedules();

    // Get current time
    String timeStr = configManager->getLocalTimeString();
    if (timeStr.indexOf("RTC not available") != -1) {
        return; // RTC not available
    }

    // Get current time from RTC (in UTC)
    DateTime nowUTC = rtcModule->getCurrentTime();
    if (!nowUTC.isValid()) {
        return;
    }

    // Convert UTC to local time for schedule comparison
    // timezoneOffset is in half-hours (e.g., 19 = UTC+9:30, 21 = UTC+10:30)
    int offsetSeconds = configManager->getTimezoneOffset() * 1800; // Convert half-hours to seconds
    if (configManager->isDaylightSaving()) {
        offsetSeconds += 3600; // Add 1 hour for DST
    }
    DateTime now = DateTime(nowUTC.unixtime() + offsetSeconds);

    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].enabled || schedules[i].id == 0) continue;

        // Check if it's time to execute this schedule
        if (isTimeMatch(schedules[i], now)) {
            Serial.printf("ScheduleManager: Executing schedule ID %d for zone %d\n",
                         schedules[i].id, schedules[i].zone);

            // Start the zone (this will handle conflicts automatically)
            ConflictResult result = startZoneManual(schedules[i].zone, schedules[i].duration);

            if (result.hasConflict) {
                Serial.printf("ScheduleManager: Schedule conflict resolved - %s\n", result.message.c_str());
            }

            // Find the active zone and mark it as scheduled
            int8_t activeSlot = findActiveZone(schedules[i].zone);
            if (activeSlot >= 0) {
                activeZones[activeSlot].isScheduled = true;
                activeZones[activeSlot].scheduleId = schedules[i].id;

                // Call zone control callback with schedule type info
                if (zoneControlCallback) {
                    zoneControlCallback(schedules[i].zone, true, schedules[i].duration, schedules[i].type, schedules[i].id);
                }
            }
        }
    }
}

ConflictResult ScheduleManager::startZoneManual(uint8_t zone, uint16_t duration) {
    ConflictResult result = {false, "", 0};

    if (!configManager || !configManager->isZoneEnabled(zone)) {
        result.hasConflict = true;
        result.message = "Zone " + String(zone) + " is not enabled";
        return result;
    }

    // Check if zone is already active
    int8_t existingSlot = findActiveZone(zone);
    if (existingSlot >= 0) {
        // Zone already running, update duration
        activeZones[existingSlot].duration = duration * 60000; // Convert to milliseconds
        activeZones[existingSlot].startTime = millis();
        result.message = "Zone " + String(zone) + " duration updated";
        return result;
    }

    // Check for conflicts and resolve if necessary
    if (getActiveZoneCount() >= MAX_ACTIVE_ZONES) {
        result = resolveZoneConflict(zone, true);
        if (result.hasConflict && result.stoppedZone == 0) {
            // Could not resolve conflict
            return result;
        }
    }

    // Find free slot and start zone
    int8_t freeSlot = findFreeActiveSlot();
    if (freeSlot >= 0) {
        activeZones[freeSlot].zone = zone;
        activeZones[freeSlot].state = RUNNING;
        activeZones[freeSlot].startTime = millis();
        activeZones[freeSlot].duration = duration * 60000; // Convert to milliseconds
        activeZones[freeSlot].isScheduled = false; // Manual start, not scheduled
        activeZones[freeSlot].scheduleId = 0;      // 0 indicates manual start
        activeZones[freeSlot].timeRemaining = duration * 60; // Duration in seconds

        // Call zone control callback (manual start, use BASIC type with scheduleId=0 to indicate manual)
        if (zoneControlCallback) {
            zoneControlCallback(zone, true, duration, BASIC, 0);
        }

        Serial.printf("ScheduleManager: Started zone %d for %d minutes (manual)\n", zone, duration);
    }

    return result;
}

bool ScheduleManager::stopZone(uint8_t zone) {
    int8_t slot = findActiveZone(zone);
    if (slot < 0) {
        return false;
    }

    // Call zone control callback (stop)
    if (zoneControlCallback) {
        zoneControlCallback(zone, false, 0, BASIC, 0);
    }

    // Clear the active zone slot
    activeZones[slot].zone = 0;
    activeZones[slot].state = IDLE;
    activeZones[slot].startTime = 0;
    activeZones[slot].duration = 0;
    activeZones[slot].isScheduled = false;
    activeZones[slot].scheduleId = 0;
    activeZones[slot].timeRemaining = 0;

    Serial.printf("ScheduleManager: Stopped zone %d\n", zone);
    return true;
}

void ScheduleManager::processActiveZones() {
    uint32_t currentTime = millis();

    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone == 0) continue;

        // Check if zone duration has expired
        if (currentTime - activeZones[i].startTime >= activeZones[i].duration) {
            uint8_t zone = activeZones[i].zone;
            stopZone(zone);
            Serial.printf("ScheduleManager: Zone %d completed its scheduled duration\n", zone);
        }
    }
}

ConflictResult ScheduleManager::resolveZoneConflict(uint8_t newZone, bool isManual) {
    ConflictResult result = {true, "", 0};

    if (getActiveZoneCount() < MAX_ACTIVE_ZONES) {
        result.hasConflict = false;
        return result;
    }

    // Find zone with least remaining time
    uint8_t zoneToStop = 0;
    uint32_t minRemainingTime = UINT32_MAX;
    int8_t slotToStop = -1;

    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone == 0) continue;

        uint32_t remainingTime = getRemainingTime(i);

        if (remainingTime < minRemainingTime) {
            minRemainingTime = remainingTime;
            zoneToStop = activeZones[i].zone;
            slotToStop = i;
        } else if (remainingTime == minRemainingTime && zoneToStop == 0) {
            // Same time, stop the first one found
            zoneToStop = activeZones[i].zone;
            slotToStop = i;
        }
    }

    if (zoneToStop > 0) {
        stopZone(zoneToStop);
        result.stoppedZone = zoneToStop;
        result.message = "Stopped zone " + String(zoneToStop) + " (least remaining time) to start zone " + String(newZone);
        Serial.printf("ScheduleManager: Conflict resolved - %s\n", result.message.c_str());
    } else {
        result.message = "Could not resolve zone conflict";
    }

    return result;
}

void ScheduleManager::cleanupExpiredAISchedules() {
    uint32_t currentTime = getCurrentUnixTime();

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == 0 || schedules[i].type != AI) continue;

        if (schedules[i].expiryTime > 0 && currentTime > schedules[i].expiryTime) {
            Serial.printf("ScheduleManager: Removing expired AI schedule ID %d\n", schedules[i].id);
            removeSchedule(schedules[i].id);
        }
    }
}

String ScheduleManager::getSchedulesJSON() {
    String json = "{\"schedules\":[";
    bool first = true;

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == 0) continue;

        if (!first) json += ",";
        first = false;

        json += "{";
        json += "\"id\":" + String(schedules[i].id) + ",";
        json += "\"zone\":" + String(schedules[i].zone) + ",";
        json += "\"days\":" + String(schedules[i].dayMask) + ",";
        json += "\"start_hour\":" + String(schedules[i].startHour) + ",";
        json += "\"start_minute\":" + String(schedules[i].startMinute) + ",";
        json += "\"duration\":" + String(schedules[i].duration) + ",";
        json += "\"enabled\":" + String(schedules[i].enabled ? "true" : "false") + ",";
        json += "\"type\":\"" + String(schedules[i].type == BASIC ? "basic" : "ai") + "\",";
        json += "\"created\":" + String(schedules[i].createdTime) + ",";
        json += "\"expires\":" + String(schedules[i].expiryTime);
        json += "}";
    }

    json += "],\"count\":" + String(scheduleCount) + "}";
    return json;
}

String ScheduleManager::getActiveZonesJSON() {
    String json = "{\"active_zones\":[";
    bool first = true;

    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone == 0) continue;

        if (!first) json += ",";
        first = false;

        uint32_t remainingTime = getRemainingTime(i);

        json += "{";
        json += "\"zone\":" + String(activeZones[i].zone) + ",";
        json += "\"remaining_seconds\":" + String(remainingTime / 1000) + ",";
        json += "\"is_scheduled\":" + String(activeZones[i].isScheduled ? "true" : "false") + ",";
        json += "\"schedule_id\":" + String(activeZones[i].scheduleId);
        json += "}";
    }

    json += "]}";
    return json;
}

// Helper methods
bool ScheduleManager::isScheduleSlotFree(uint8_t index) {
    return (index < MAX_SCHEDULES && schedules[index].id == 0);
}

uint8_t ScheduleManager::findScheduleById(uint8_t id) {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == id) {
            return i;
        }
    }
    return MAX_SCHEDULES; // Not found
}

uint8_t ScheduleManager::findFreeScheduleSlot() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == 0) {
            return i;
        }
    }
    return MAX_SCHEDULES; // No free slots
}

int8_t ScheduleManager::findActiveZone(uint8_t zone) {
    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone == zone) {
            return i;
        }
    }
    return -1; // Not found
}

int8_t ScheduleManager::findFreeActiveSlot() {
    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone == 0) {
            return i;
        }
    }
    return -1; // No free slots
}

uint8_t ScheduleManager::getActiveZoneCount() {
    uint8_t count = 0;
    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone != 0) {
            count++;
        }
    }
    return count;
}

uint32_t ScheduleManager::getRemainingTime(uint8_t activeIndex) {
    if (activeIndex >= MAX_ACTIVE_ZONES || activeZones[activeIndex].zone == 0) {
        return 0;
    }

    uint32_t elapsed = millis() - activeZones[activeIndex].startTime;
    if (elapsed >= activeZones[activeIndex].duration) {
        return 0;
    }

    return activeZones[activeIndex].duration - elapsed;
}

bool ScheduleManager::isTimeMatch(const ScheduleEntry& schedule, const DateTime& now) {
    // Check day of week (0=Sunday, 6=Saturday)
    uint8_t dayBit = 1 << now.dayOfTheWeek();
    if (!(schedule.dayMask & dayBit)) {
        return false;
    }

    // Check hour and minute
    return (schedule.startHour == now.hour() && schedule.startMinute == now.minute());
}

uint32_t ScheduleManager::getCurrentUnixTime() {
    if (!configManager) return 0;

    // This would need to be implemented based on your RTC setup
    // For now, return a placeholder
    return millis() / 1000; // Simplified - should use actual RTC time
}

void ScheduleManager::setZoneControlCallback(void (*callback)(uint8_t zone, bool state, uint16_t duration, ScheduleType schedType, uint8_t schedId)) {
    zoneControlCallback = callback;
}

bool ScheduleManager::setAIScheduleBatch(const String& jsonSchedules) {
    // Clear existing AI schedules
    clearAISchedules();

    // Parse JSON and add new AI schedules
    // This would parse the Node-RED JSON format
    // Implementation depends on your specific JSON format
    Serial.println("ScheduleManager: AI schedule batch update received");
    return true;
}

void ScheduleManager::clearAISchedules() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id != 0 && schedules[i].type == AI) {
            removeSchedule(schedules[i].id);
        }
    }
    Serial.println("ScheduleManager: Cleared all AI schedules");
}

bool ScheduleManager::hasActiveZones() {
    return getActiveZoneCount() > 0;
}

// Rain control methods
void ScheduleManager::setRainDelay(uint32_t delayMinutes) {
    rainDelayActive = true;
    DateTime now = rtcModule->getCurrentTime();
    rainDelayEndTime = now.unixtime() + (delayMinutes * 60);
    Serial.println("ScheduleManager: Rain delay set for " + String(delayMinutes) + " minutes");
}

void ScheduleManager::cancelZoneForRain(uint8_t zone) {
    int8_t activeIndex = findActiveZone(zone);
    if (activeIndex >= 0) {
        activeZones[activeIndex].state = RAINCANCELLED;
        if (zoneControlCallback) {
            zoneControlCallback(zone, false, 0, BASIC, 0);
        }
        Serial.println("ScheduleManager: Zone " + String(zone) + " cancelled due to rain");
    }
}

void ScheduleManager::clearRainDelay() {
    rainDelayActive = false;
    rainDelayEndTime = 0;
    Serial.println("ScheduleManager: Rain delay cleared");
}

// Status reporting for Node-RED interface
String ScheduleManager::getDeviceStatusJSON() {
    JsonDocument doc;

    // Get current time with timezone
    DateTime utcTime = rtcModule->getCurrentTime();
    int offsetSeconds = configManager->getTimezoneOffset() * 1800; // Convert half-hours to seconds
    if (configManager->isDaylightSaving()) {
        offsetSeconds += 3600; // Add 1 hour for DST
    }
    DateTime localTime = DateTime(utcTime.unixtime() + offsetSeconds);

    char timeBuffer[32];
    int offsetHours = offsetSeconds / 3600;
    int offsetMins = abs(offsetSeconds) % 3600 / 60;
    sprintf(timeBuffer, "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
            localTime.year(), localTime.month(), localTime.day(),
            localTime.hour(), localTime.minute(), localTime.second(),
            offsetHours, offsetMins);
    doc["timestamp"] = timeBuffer;

    doc["scheduleEnabled"] = scheduleEnabled && !rainDelayActive;
    doc["rainDelayActive"] = rainDelayActive;
    if (rainDelayActive) {
        doc["rainDelayEnd"] = rainDelayEndTime;
    }

    // Zone status array
    JsonArray zones = doc["zones"].to<JsonArray>();
    for (int i = 0; i < MAX_ACTIVE_ZONES; i++) {
        if (activeZones[i].zone > 0) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = activeZones[i].zone;

            // Convert state to string
            const char* stateStr = "idle";
            switch (activeZones[i].state) {
                case SCHEDULED: stateStr = "scheduled"; break;
                case RUNNING: stateStr = "running"; break;
                case COMPLETED: stateStr = "completed"; break;
                case RAINDELAYED: stateStr = "raindelayed"; break;
                case RAINCANCELLED: stateStr = "raincancelled"; break;
                default: stateStr = "idle"; break;
            }
            zone["status"] = stateStr;

            if (activeZones[i].state == RUNNING) {
                uint32_t elapsed = millis() - activeZones[i].startTime;
                uint32_t remaining = (activeZones[i].duration > elapsed) ?
                                   (activeZones[i].duration - elapsed) / 1000 : 0;
                zone["timeRemaining"] = remaining;
            }
        }
    }

    String result;
    serializeJson(doc, result);
    return result;
}

String ScheduleManager::getNextEventJSON() {
    JsonDocument doc;

    // Find next scheduled event
    // Get current time with timezone
    DateTime utcTime = rtcModule->getCurrentTime();
    int offsetSeconds = configManager->getTimezoneOffset() * 1800; // Convert half-hours to seconds
    if (configManager->isDaylightSaving()) {
        offsetSeconds += 3600; // Add 1 hour for DST
    }
    DateTime localTime = DateTime(utcTime.unixtime() + offsetSeconds);

    uint8_t nextZone = 0;
    uint32_t nextTime = 0;

    // Look through all enabled schedules to find the next one
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].enabled && schedules[i].id != 0) {
            // Calculate next occurrence for this schedule
            // This is simplified - full implementation would check day masks
            uint32_t scheduleTime = schedules[i].startHour * 3600 + schedules[i].startMinute * 60;
            uint32_t currentTime = localTime.hour() * 3600 + localTime.minute() * 60;

            if (scheduleTime > currentTime) {
                if (nextTime == 0 || scheduleTime < nextTime) {
                    nextTime = scheduleTime;
                    nextZone = schedules[i].zone;
                }
            }
        }
    }

    if (nextZone > 0) {
        doc["zone"] = nextZone;
        char timeStr[16];
        sprintf(timeStr, "%02d:%02d", nextTime / 3600, (nextTime % 3600) / 60);
        doc["time"] = timeStr;
    }

    String result;
    serializeJson(doc, result);
    return result;
}

bool ScheduleManager::updateScheduleFromJSON(const String& jsonCommand) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonCommand);

    if (error) {
        Serial.println("ScheduleManager: JSON parse error: " + String(error.c_str()));
        return false;
    }

    String command = doc["command"];

    if (command == "updateSchedule") {
        // Handle full schedule update from Node-RED
        clearAISchedules();

        if (doc["days"].is<JsonArray>()) {
            JsonArray days = doc["days"];
            // Process 7-day schedule array
            // Implementation would parse the full schedule structure
        }

        Serial.println("ScheduleManager: Schedule updated from Node-RED");
        return true;
    } else if (command == "rainDelay") {
        uint32_t minutes = doc["minutes"];
        setRainDelay(minutes);
        return true;
    } else if (command == "cancelRain") {
        clearRainDelay();
        return true;
    } else if (command == "enableSchedule") {
        scheduleEnabled = doc["enabled"];
        Serial.println("ScheduleManager: Schedule " + String(scheduleEnabled ? "enabled" : "disabled"));
        return true;
    }

    return false;
}