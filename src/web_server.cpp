#include "web_server.h"
#include "hunter_esp32.h"
#include "rtc_module.h"
#include "config_manager.h"

// HTML interface for irrigation control
const char* getMainHTML() {
    return "<!DOCTYPE html>"
           "<html><head>"
           "<title>Hunter ESP32 Irrigation Controller</title>"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
           "<style>"
           "body{font-family:Arial;margin:20px;background:#f0f8ff;}"
           ".container{max-width:800px;margin:0 auto;}"
           ".card{background:white;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
           ".zone-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px;}"
           ".zone-card{background:#e8f4f8;padding:15px;border-radius:5px;text-align:center;}"
           ".zone-active{background:#90ee90;}"
           ".zone-inactive{background:#ffcccb;}"
           "button{padding:10px 15px;margin:5px;border:none;border-radius:4px;cursor:pointer;}"
           ".start-btn{background:#4CAF50;color:white;}"
           ".stop-btn{background:#f44336;color:white;}"
           ".program-btn{background:#2196F3;color:white;}"
           "input[type=\"number\"]{padding:8px;margin:5px;border:1px solid #ccc;border-radius:4px;}"
           ".status{font-weight:bold;color:#2196F3;}"
           ".time-display{font-size:18px;color:#333;}"
           "</style></head><body>"
           "<div class=\"container\">"
           "<div class=\"card\">"
           "<h1>🌱 Hunter ESP32 Irrigation Controller</h1>"
           "<div class=\"status\" id=\"status\">System Ready</div>"
           "<div class=\"time-display\" id=\"currentTime\">Loading time...</div>"
           "</div>"
           "<div class=\"card\">"
           "<h2>Zone Control</h2>"
           "<div class=\"zone-grid\" id=\"zoneGrid\"></div>"
           "</div>"
           "<div class=\"card\">"
           "<h2>Program Control</h2>"
           "<button class=\"program-btn\" onclick=\"runProgram(1)\">Program 1</button>"
           "<button class=\"program-btn\" onclick=\"runProgram(2)\">Program 2</button>"
           "<button class=\"program-btn\" onclick=\"runProgram(3)\">Program 3</button>"
           "<button class=\"program-btn\" onclick=\"runProgram(4)\">Program 4</button>"
           "</div>"
           "<div class=\"card\">"
           "<h2>System Status</h2>"
           "<div id=\"systemInfo\">Loading...</div>"
           "</div>"
           "</div>"
           "<script>"
           "function initZones(){"
           "const grid=document.getElementById('zoneGrid');"
           "for(let i=1;i<=16;i++){"
           "const zoneCard=document.createElement('div');"
           "zoneCard.className='zone-card zone-inactive';"
           "zoneCard.innerHTML='<h3>Zone '+i+'</h3><input type=\"number\" id=\"time'+i+'\" value=\"5\" min=\"1\" max=\"240\" placeholder=\"Minutes\"><br><button class=\"start-btn\" onclick=\"startZone('+i+')\">Start</button><button class=\"stop-btn\" onclick=\"stopZone('+i+')\">Stop</button>';"
           "grid.appendChild(zoneCard);}}"
           "function startZone(zone){"
           "const time=document.getElementById('time'+zone).value;"
           "fetch('/api/start-zone?zone='+zone+'&time='+time).then(response=>response.text()).then(data=>{"
           "document.getElementById('status').textContent='Zone '+zone+' started for '+time+' minutes';"
           "updateZoneStatus(zone,true);}).catch(err=>console.error('Error:',err));}"
           "function stopZone(zone){"
           "fetch('/api/stop-zone?zone='+zone).then(response=>response.text()).then(data=>{"
           "document.getElementById('status').textContent='Zone '+zone+' stopped';"
           "updateZoneStatus(zone,false);}).catch(err=>console.error('Error:',err));}"
           "function runProgram(program){"
           "fetch('/api/run-program?program='+program).then(response=>response.text()).then(data=>{"
           "document.getElementById('status').textContent='Program '+program+' started';}).catch(err=>console.error('Error:',err));}"
           "function updateZoneStatus(zone,active){"
           "const zoneCard=document.querySelectorAll('.zone-card')[zone-1];"
           "zoneCard.className=active?'zone-card zone-active':'zone-card zone-inactive';}"
           "function updateTime(){"
           "fetch('/api/time').then(response=>response.text()).then(time=>{"
           "document.getElementById('currentTime').textContent='Current Time: '+time;}).catch(err=>console.error('Error:',err));}"
           "function updateSystemInfo(){"
           "fetch('/api/status').then(response=>response.text()).then(info=>{"
           "document.getElementById('systemInfo').innerHTML=info;}).catch(err=>console.error('Error:',err));}"
           "document.addEventListener('DOMContentLoaded',function(){"
           "initZones();updateTime();updateSystemInfo();"
           "setInterval(updateTime,10000);setInterval(updateSystemInfo,30000);});"
           "</script></body></html>";
}

// Static members
int HunterWebServer::globalZoneID = 0;
int HunterWebServer::globalProgramID = 0;
int HunterWebServer::globalTime = 0;
int HunterWebServer::globalFlag = 0;
const int HunterWebServer::PUMP_PIN = 5;
RTCModule* HunterWebServer::rtcModule = nullptr;
ConfigManager* HunterWebServer::configManager = nullptr;
ZoneSchedule HunterWebServer::schedules[16] = {}; // Initialize all to default values
int HunterWebServer::activeZones[16] = {}; // All zones start inactive
unsigned long HunterWebServer::zoneStartTimes[16] = {}; // All start times zero
int HunterWebServer::zoneDurations[16] = {}; // All durations zero
static HunterWebServer* serverInstance = nullptr;

HunterWebServer::HunterWebServer(int port) : server(port) {
    serverInstance = this;
}

void HunterWebServer::begin() {
    // Main page
    server.on("/", HTTP_GET, handleRoot);

    // REST API endpoints
    server.on("/api/start-zone", HTTP_GET, handleStartZone);
    server.on("/api/stop-zone", HTTP_GET, handleStopZone);
    server.on("/api/run-program", HTTP_GET, handleRunProgram);
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/set-time", HTTP_POST, handleSetTime);
    server.on("/api/sync-ntp", HTTP_GET, handleSyncNTP);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_POST, handleSetConfig);

    // 404 handler
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Hunter ESP32 WebServer started with REST API");
    Serial.println("Available endpoints:");
    Serial.println("  GET  /                    - Main irrigation control interface");
    Serial.println("  GET  /api/start-zone      - Start zone (params: zone, time)");
    Serial.println("  GET  /api/stop-zone       - Stop zone (params: zone)");
    Serial.println("  GET  /api/run-program     - Run program (params: program)");
    Serial.println("  GET  /api/time            - Get current time");
    Serial.println("  GET  /api/status          - Get system status");
    Serial.println("  GET  /api/sync-ntp        - Sync RTC with NTP time");
    Serial.println("  GET  /api/config          - Get system configuration");
    Serial.println("  POST /api/config          - Set system configuration");
    Serial.println("  POST /api/set-time        - Set current time");
}

void HunterWebServer::handleRoot() {
    if (serverInstance) {
        serverInstance->server.send(200, "text/html", getMainHTML());
        Serial.println("Served main irrigation control interface");
    }
}

void HunterWebServer::handleNotFound() {
    if (serverInstance) {
        serverInstance->server.send(404, "text/plain", "Not found");
    }
}

void HunterWebServer::processCommands() {
    server.handleClient();

    // Check zone timers for automatic shutoff
    if (serverInstance) {
        serverInstance->checkZoneTimers();
        serverInstance->checkSchedules();
    }

    // Process any pending commands from web interface
    if (globalFlag > 0) {
        switch (globalFlag) {
            case 1: // Start zone
                Serial.printf("Processing command: Start Zone %d for %d minutes\n", globalZoneID, globalTime);
                HunterStart(globalZoneID, globalTime);
                break;

            case 2: // Stop zone
                Serial.printf("Processing command: Stop Zone %d\n", globalZoneID);
                HunterStop(globalZoneID);
                break;

            case 3: // Run program
                Serial.printf("Processing command: Run Program %d\n", globalProgramID);
                HunterProgram(globalProgramID);
                break;

            default:
                Serial.printf("Unknown command flag: %d\n", globalFlag);
                break;
        }

        // Clear the flag after processing
        clearFlag();
    }
}void HunterWebServer::handleStartZone() {
    if (!serverInstance) return;

    String zone = serverInstance->server.arg("zone");
    String time = serverInstance->server.arg("time");

    if (zone.length() == 0 || time.length() == 0) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Missing zone or time parameter\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    int zoneNum = zone.toInt();
    int timeMin = time.toInt();

    if (zoneNum < 1 || zoneNum > 48) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Zone must be 1-48\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    // Check if zone is enabled
    if (configManager && !configManager->isZoneEnabled(zoneNum)) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Zone " + String(zoneNum) + " is not enabled. Maximum enabled zones: " + String(configManager->getMaxEnabledZones()) + "\"}";
        serverInstance->server.send(403, "application/json", jsonError);
        return;
    }

    if (timeMin < 1 || timeMin > 240) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Time must be 1-240 minutes\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    // Set global variables for main loop to process
    globalZoneID = zoneNum;
    globalTime = timeMin;
    globalFlag = 1; // Flag for start zone command

    // Start the zone timer
    if (serverInstance) {
        serverInstance->startZoneTimer(zoneNum, timeMin);
    }

    // Turn on pump
    digitalWrite(PUMP_PIN, HIGH);

    String jsonResponse = "{\"status\":\"success\",\"message\":\"Zone " + String(zoneNum) + " started for " + String(timeMin) + " minutes\",\"zone\":" + String(zoneNum) + ",\"duration_minutes\":" + String(timeMin) + "}";
    serverInstance->server.send(200, "application/json", jsonResponse);

    Serial.println("API: Zone " + String(zoneNum) + " started for " + String(timeMin) + " minutes");
}

void HunterWebServer::handleStopZone() {
    if (!serverInstance) return;

    String zone = serverInstance->server.arg("zone");

    if (zone.length() == 0) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Missing zone parameter\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    int zoneNum = zone.toInt();

    if (zoneNum < 1 || zoneNum > 48) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Zone must be 1-48\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    // Check if zone is enabled
    if (configManager && !configManager->isZoneEnabled(zoneNum)) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Zone " + String(zoneNum) + " is not enabled. Maximum enabled zones: " + String(configManager->getMaxEnabledZones()) + "\"}";
        serverInstance->server.send(403, "application/json", jsonError);
        return;
    }

    // Set global variables for main loop to process
    globalZoneID = zoneNum;
    globalTime = 0; // 0 time means stop
    globalFlag = 2; // Flag for stop zone command

    // Stop the zone timer
    if (serverInstance) {
        serverInstance->stopZoneTimer(zoneNum);
    }

    // Check if all zones are stopped, if so turn off pump
    bool anyZoneActive = false;
    for (int i = 0; i < 16; i++) {
        if (activeZones[i] > 0) {
            anyZoneActive = true;
            break;
        }
    }
    if (!anyZoneActive) {
        digitalWrite(PUMP_PIN, LOW);
    }

    String jsonResponse = "{\"status\":\"success\",\"message\":\"Zone " + String(zoneNum) + " stopped\",\"zone\":" + String(zoneNum) + "}";
    serverInstance->server.send(200, "application/json", jsonResponse);

    Serial.println("API: Zone " + String(zoneNum) + " stopped");
}

void HunterWebServer::handleRunProgram() {
    if (!serverInstance) return;

    String program = serverInstance->server.arg("program");

    if (program.length() == 0) {
        serverInstance->server.send(400, "text/plain", "ERROR: Missing program parameter");
        return;
    }

    int progNum = program.toInt();

    if (progNum < 1 || progNum > 4) {
        serverInstance->server.send(400, "text/plain", "ERROR: Program must be 1-4");
        return;
    }

    // Set global variables for main loop to process
    globalProgramID = progNum;
    globalFlag = 3; // Flag for run program command

    // Turn on pump for program
    digitalWrite(PUMP_PIN, HIGH);

    String response = "Program " + String(progNum) + " started";
    serverInstance->server.send(200, "text/plain", response);

    Serial.println("API: " + response);
}

void HunterWebServer::handleGetTime() {
    if (!serverInstance) return;

    if (configManager) {
        String localTime = configManager->getLocalTimeString();
        String jsonResponse = "{\"status\":\"success\",\"time\":\"" + localTime + "\"}";
        serverInstance->server.send(200, "application/json", jsonResponse);
    } else if (rtcModule) {
        String currentTime = rtcModule->getDateTimeString();
        String jsonResponse = "{\"status\":\"success\",\"time\":\"" + currentTime + " (UTC)\"}";
        serverInstance->server.send(200, "application/json", jsonResponse);
    } else {
        String jsonError = "{\"status\":\"error\",\"message\":\"RTC module not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
    }
}

void HunterWebServer::handleGetStatus() {
    if (!serverInstance) return;

    String jsonResponse = "{";
    jsonResponse += "\"status\":\"success\",";
    jsonResponse += "\"system\":{";
    jsonResponse += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    jsonResponse += "\"uptime_seconds\":" + String(millis() / 1000) + ",";
    jsonResponse += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    jsonResponse += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
    jsonResponse += "\"mac_address\":\"" + WiFi.macAddress() + "\"";
    jsonResponse += "},";

    // Pump status
    jsonResponse += "\"pump\":{";
    jsonResponse += "\"status\":\"" + String(digitalRead(PUMP_PIN) ? "ON" : "OFF") + "\"";
    jsonResponse += "},";

    // RTC status
    jsonResponse += "\"rtc\":{";
    if (rtcModule) {
        jsonResponse += "\"status\":\"connected\",";
        if (configManager) {
            String localTime = configManager->getLocalTimeString();
            jsonResponse += "\"local_time\":\"" + localTime + "\"";
        } else {
            String utcTime = rtcModule->getDateTimeString();
            jsonResponse += "\"utc_time\":\"" + utcTime + "\"";
        }
    } else {
        jsonResponse += "\"status\":\"not_available\"";
    }
    jsonResponse += "}";

    jsonResponse += "}";

    serverInstance->server.send(200, "application/json", jsonResponse);
}

void HunterWebServer::handleSetTime() {
    if (!serverInstance) return;

    // This would typically parse JSON body for time setting
    // For now, just acknowledge the request
    String response = "Time setting not yet implemented via web interface";
    serverInstance->server.send(501, "text/plain", response);
}

void HunterWebServer::handleSyncNTP() {
    if (!serverInstance) return;

    if (!rtcModule) {
        String jsonError = "{\"status\":\"error\",\"message\":\"RTC module not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        String jsonError = "{\"status\":\"error\",\"message\":\"WiFi not connected\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    Serial.println("API: Manual NTP sync requested");

    // Use configuration manager settings if available
    if (configManager) {
        bool success = rtcModule->syncWithNTP(
            configManager->getNTPServer1().c_str(),
            configManager->getNTPServer2().c_str(),
            configManager->getTimezoneOffset()
        );

        if (success) {
            String localTime = configManager->getLocalTimeString();
            String jsonResponse = "{\"status\":\"success\",\"message\":\"RTC synchronized with NTP time\",\"local_time\":\"" + localTime + "\"}";
            serverInstance->server.send(200, "application/json", jsonResponse);
            Serial.println("API: NTP sync successful with configured timezone");
        } else {
            String jsonError = "{\"status\":\"error\",\"message\":\"Failed to synchronize with NTP servers\"}";
            serverInstance->server.send(500, "application/json", jsonError);
            Serial.println("API: NTP sync failed");
        }
    } else {
        // Fallback to default NTP sync
        if (rtcModule->syncWithNTP()) {
            String utcTime = rtcModule->getDateTimeString();
            String jsonResponse = "{\"status\":\"success\",\"message\":\"RTC synchronized with NTP time\",\"utc_time\":\"" + utcTime + "\"}";
            serverInstance->server.send(200, "application/json", jsonResponse);
            Serial.println("API: NTP sync successful");
        } else {
            String jsonError = "{\"status\":\"error\",\"message\":\"Failed to synchronize with NTP servers\"}";
            serverInstance->server.send(500, "application/json", jsonError);
            Serial.println("API: NTP sync failed");
        }
    }
}

void HunterWebServer::handleGetConfig() {
    if (!serverInstance) return;

    if (!configManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Configuration manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    String configJSON = configManager->getConfigJSON();
    serverInstance->server.send(200, "application/json", configJSON);
    Serial.println("API: Configuration retrieved");
}

void HunterWebServer::handleSetConfig() {
    if (!serverInstance) return;

    if (!configManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Configuration manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    // Parse URL parameters for configuration updates
    String response = "Configuration updated:\n";
    bool configChanged = false;

    // Timezone settings
    if (serverInstance->server.hasArg("timezone")) {
        float timezoneFloat = serverInstance->server.arg("timezone").toFloat();
        int timezoneHalfHours = (int)(timezoneFloat * 2);  // Convert to half-hour increments
        if (timezoneHalfHours >= -24 && timezoneHalfHours <= 28) {
            configManager->setTimezoneOffset(timezoneHalfHours);
            int hours = timezoneHalfHours / 2;
            int minutes = (timezoneHalfHours % 2) * 30;
            response += "- Timezone: UTC" + String(hours >= 0 ? "+" : "") + String(hours) + ":" +
                       (minutes < 10 ? "0" : "") + String(minutes) + "\n";
            configChanged = true;
        }
    }

    if (serverInstance->server.hasArg("daylight_saving")) {
        bool dst = (serverInstance->server.arg("daylight_saving") == "true");
        configManager->setDaylightSaving(dst);
        response += "- Daylight Saving: " + String(dst ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    // NTP settings
    if (serverInstance->server.hasArg("ntp_server1")) {
        String server1 = serverInstance->server.arg("ntp_server1");
        String server2 = serverInstance->server.hasArg("ntp_server2") ?
                         serverInstance->server.arg("ntp_server2") : "time.nist.gov";
        configManager->setNTPServers(server1, server2);
        response += "- NTP Servers: " + server1 + ", " + server2 + "\n";
        configChanged = true;
    }

    if (serverInstance->server.hasArg("auto_ntp")) {
        bool autoNTP = (serverInstance->server.arg("auto_ntp") == "true");
        configManager->setAutoNTPSync(autoNTP);
        response += "- Auto NTP Sync: " + String(autoNTP ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    if (serverInstance->server.hasArg("sync_interval")) {
        int interval = serverInstance->server.arg("sync_interval").toInt();
        if (interval > 0 && interval <= 168) {
            configManager->setSyncInterval(interval);
            response += "- Sync Interval: " + String(interval) + " hours\n";
            configChanged = true;
        }
    }

    // Irrigation settings
    if (serverInstance->server.hasArg("max_runtime")) {
        int maxRuntime = serverInstance->server.arg("max_runtime").toInt();
        if (maxRuntime > 0 && maxRuntime <= 1440) {
            configManager->setMaxZoneRunTime(maxRuntime);
            response += "- Max Zone Runtime: " + String(maxRuntime) + " minutes\n";
            configChanged = true;
        }
    }

    if (serverInstance->server.hasArg("max_enabled_zones")) {
        int maxZones = serverInstance->server.arg("max_enabled_zones").toInt();
        if (maxZones >= 1 && maxZones <= 16) {
            configManager->setMaxEnabledZones(maxZones);
            response += "- Max Enabled Zones: " + String(maxZones) + "\n";
            configChanged = true;
        }
    }

    if (serverInstance->server.hasArg("pump_safety")) {
        bool pumpSafety = (serverInstance->server.arg("pump_safety") == "true");
        configManager->setPumpSafetyMode(pumpSafety);
        response += "- Pump Safety Mode: " + String(pumpSafety ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    if (configChanged) {
        if (configManager->saveConfig()) {
            String jsonResponse = "{\"status\":\"success\",\"message\":\"Configuration updated successfully\",\"config\":" + configManager->getConfigJSON() + "}";
            serverInstance->server.send(200, "application/json", jsonResponse);
            Serial.println("API: Configuration updated and saved");
        } else {
            String jsonError = "{\"status\":\"error\",\"message\":\"Failed to save configuration\"}";
            serverInstance->server.send(500, "application/json", jsonError);
            Serial.println("API: Configuration update failed - save error");
        }
    } else {
        String jsonError = "{\"status\":\"error\",\"message\":\"No valid configuration parameters provided\"}";
        serverInstance->server.send(400, "application/json", jsonError);
    }
}

// Zone timer management methods
void HunterWebServer::startZoneTimer(int zone, int duration) {
    if (zone < 1 || zone > 16) return;

    int index = zone - 1;
    activeZones[index] = zone;
    zoneStartTimes[index] = millis();
    zoneDurations[index] = duration * 60 * 1000; // Convert minutes to milliseconds

    Serial.printf("Zone %d timer started for %d minutes\n", zone, duration);
}

void HunterWebServer::stopZoneTimer(int zone) {
    if (zone < 1 || zone > 16) return;

    int index = zone - 1;
    activeZones[index] = 0;
    zoneStartTimes[index] = 0;
    zoneDurations[index] = 0;

    Serial.printf("Zone %d timer stopped\n", zone);
}

bool HunterWebServer::isZoneActive(int zone) {
    if (zone < 1 || zone > 16) return false;
    return activeZones[zone - 1] > 0;
}

void HunterWebServer::checkZoneTimers() {
    unsigned long currentTime = millis();
    bool anyZoneActive = false;

    for (int i = 0; i < 16; i++) {
        if (activeZones[i] > 0) {
            // Check if this zone has exceeded its duration
            if (currentTime - zoneStartTimes[i] >= zoneDurations[i]) {
                int zone = i + 1;
                Serial.printf("Zone %d timer expired, stopping zone\n", zone);

                // Stop the zone
                HunterStop(zone);
                stopZoneTimer(zone);
            } else {
                anyZoneActive = true;
            }
        }
    }

    // Turn off pump if no zones are active
    if (!anyZoneActive && digitalRead(PUMP_PIN) == HIGH) {
        digitalWrite(PUMP_PIN, LOW);
        Serial.println("All zones stopped, pump turned off");
    }
}

// Scheduling system methods
void HunterWebServer::addSchedule(int zone, int hour, int minute, int duration) {
    if (zone < 1 || zone > 16) return;

    int index = zone - 1;
    schedules[index].zoneNumber = zone;
    schedules[index].hour = hour;
    schedules[index].minute = minute;
    schedules[index].duration = duration;
    schedules[index].enabled = true;
    schedules[index].isActive = false;

    Serial.printf("Schedule added for Zone %d at %02d:%02d for %d minutes\n", zone, hour, minute, duration);
}

void HunterWebServer::removeSchedule(int zone) {
    if (zone < 1 || zone > 16) return;

    int index = zone - 1;
    schedules[index] = {}; // Reset to default values

    Serial.printf("Schedule removed for Zone %d\n", zone);
}

void HunterWebServer::enableSchedule(int zone, bool enabled) {
    if (zone < 1 || zone > 16) return;

    int index = zone - 1;
    schedules[index].enabled = enabled;

    Serial.printf("Zone %d schedule %s\n", zone, enabled ? "enabled" : "disabled");
}

void HunterWebServer::checkSchedules() {
    if (!rtcModule || !rtcModule->isRunning()) return;

    DateTime now = rtcModule->getCurrentTime();
    int currentHour = now.hour();
    int currentMinute = now.minute();

    for (int i = 0; i < 16; i++) {
        ZoneSchedule &schedule = schedules[i];

        if (schedule.enabled && schedule.zoneNumber > 0) {
            // Check if it's time to start this zone
            if (currentHour == schedule.hour && currentMinute == schedule.minute && !schedule.isActive) {
                // Start the zone
                int zone = schedule.zoneNumber;
                Serial.printf("Schedule triggered: Starting Zone %d for %d minutes\n", zone, schedule.duration);

                startZoneTimer(zone, schedule.duration);
                HunterStart(zone, schedule.duration);

                schedule.isActive = true;
                schedule.startTime = millis();
            }

            // Reset the active flag after the minute has passed to allow for next day
            if (schedule.isActive && (currentHour != schedule.hour || currentMinute != schedule.minute)) {
                schedule.isActive = false;
            }
        }
    }
}
