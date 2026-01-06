#include "web_server.h"
#include "hunter_esp32.h"
#include "rtc_module.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "event_logger.h"
#include "http_client.h"
#include "mqtt_manager.h"
#include "build_number.h"
#include <ArduinoJson.h>

// HTML interface for irrigation control
const char* getMainHTML() {
    return "<!DOCTYPE html>"
           "<html><head>"
           "<title>ESP32 Irrigation Controller</title>"
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
           ".collapsible{cursor:pointer;padding:10px;background:#2196F3;color:white;border:none;text-align:left;width:100%;font-size:16px;border-radius:4px;}"
           ".collapsible:hover{background:#1976D2;}"
           ".collapsible:after{content:'\\25BC';float:right;}"
           ".collapsible.active:after{content:'\\25B2';}"
           ".collapse-content{max-height:0;overflow:hidden;transition:max-height 0.3s ease-out;}"
           "</style></head><body>"
           "<div class=\"container\">"
           "<div class=\"card\">"
           "<h1>ESP32 Irrigation Controller</h1>"
           "<div class=\"status\" id=\"status\">System Ready</div>"
           "<div class=\"time-display\" id=\"currentTime\">Loading time...</div>"
           "<button class=\"program-btn\" onclick=\"fetchSchedules()\" style=\"margin-top:10px;width:100%;\">Fetch 5 Day Schedule</button>"
           "</div>"
           "<div class=\"card\">"
           "<button class=\"collapsible\" onclick=\"toggleCollapse(this)\">System Status</button>"
           "<div class=\"collapse-content\" id=\"systemInfo\">Loading...</div>"
           "</div>"
           "<div class=\"card\">"
           "<button class=\"collapsible\" onclick=\"toggleCollapse(this)\">Current Schedules</button>"
           "<div class=\"collapse-content\" id=\"scheduleInfo\"><div style=\"padding:10px;\">Loading...</div></div>"
           "</div>"
           "</div>"
           "<div class=\"card\">"
           "<h2>Zone Control</h2>"
           "<div class=\"zone-grid\" id=\"zoneGrid\"></div>"
           "</div>"
           "<script>"
           "function initZones(){"
           "const grid=document.getElementById('zoneGrid');"
           "for(let i=1;i<=12;i++){"
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
           "fetch('/api/time').then(response=>response.json()).then(data=>{"
           "document.getElementById('currentTime').textContent='Current Time: '+(data.time||'N/A');}).catch(err=>console.error('Error:',err));}"
           "function updateSystemInfo(){"
           "fetch('/api/status').then(response=>response.json()).then(data=>{"
           "let html='<table style=\"width:100%;border-collapse:collapse\">';"
           "if(data.system){for(let key in data.system){html+='<tr><td style=\"padding:8px;border:1px solid #ddd;font-weight:bold\">'+key.replace(/_/g,' ').toUpperCase()+'</td><td style=\"padding:8px;border:1px solid #ddd\">'+data.system[key]+'</td></tr>';}}"
           "if(data.pump){html+='<tr><td style=\"padding:8px;border:1px solid #ddd;font-weight:bold\">PUMP STATUS</td><td style=\"padding:8px;border:1px solid #ddd\">'+data.pump.status+'</td></tr>';}"
           "if(data.rtc){for(let key in data.rtc){html+='<tr><td style=\"padding:8px;border:1px solid #ddd;font-weight:bold\">RTC '+key.replace(/_/g,' ').toUpperCase()+'</td><td style=\"padding:8px;border:1px solid #ddd\">'+data.rtc[key]+'</td></tr>';}}"
           "if(data.mqtt){for(let key in data.mqtt){var label=key.replace(/^mqtt_/,'').replace(/_/g,' ').toUpperCase();html+='<tr><td style=\"padding:8px;border:1px solid #ddd;font-weight:bold\">MQTT '+label+'</td><td style=\"padding:8px;border:1px solid #ddd\">'+data.mqtt[key]+'</td></tr>';}}"
           "html+='</table>';document.getElementById('systemInfo').innerHTML=html;}).catch(err=>console.error('Error:',err));}"
           "function toggleCollapse(btn){"
           "btn.classList.toggle('active');"
           "var content=btn.nextElementSibling;"
           "if(content.style.maxHeight){content.style.maxHeight=null;}else{content.style.maxHeight=content.scrollHeight+'px';}}"
           "function fetchSchedules(){"
           "document.getElementById('status').textContent='Fetching 5-day schedule...';"
           "fetch('/api/schedules/fetch?days=5',{method:'POST'}).then(response=>response.json()).then(data=>{"
           "if(data.status=='success'){document.getElementById('status').textContent='Schedule fetched successfully ('+data.days+' days)';updateScheduleInfo();}else{"
           "document.getElementById('status').textContent='Fetch failed: '+data.message;}}).catch(err=>{"
           "document.getElementById('status').textContent='Fetch error: '+err.message;console.error('Error:',err);});}"
           "function updateScheduleInfo(){"
           "fetch('/api/schedules').then(response=>response.json()).then(data=>{"
           "if(!data.schedules||data.schedules.length==0){document.getElementById('scheduleInfo').innerHTML='<div style=\"padding:10px;text-align:center;color:#999;\">No schedules found</div>';return;}"
           "let html='<table style=\"width:100%;border-collapse:collapse;font-size:0.9em;\">';"
           "html+='<thead><tr style=\"background:#2196F3;color:white;\"><th style=\"padding:8px;text-align:left;\">Zone</th><th style=\"padding:8px;text-align:left;\">Time</th><th style=\"padding:8px;text-align:left;\">Duration</th><th style=\"padding:8px;text-align:left;\">Days</th><th style=\"padding:8px;text-align:left;\">Type</th></tr></thead><tbody>';"
           "data.schedules.forEach(s=>{"
           "let type=s.type==1?'AI':'Basic';"
           "let days='';"
           "if(s.day_mask&1)days+='Su ';"
           "if(s.day_mask&2)days+='Mo ';"
           "if(s.day_mask&4)days+='Tu ';"
           "if(s.day_mask&8)days+='We ';"
           "if(s.day_mask&16)days+='Th ';"
           "if(s.day_mask&32)days+='Fr ';"
           "if(s.day_mask&64)days+='Sa ';"
           "if(days=='')days='None';"
           "let time=String(s.start_hour).padStart(2,'0')+':'+String(s.start_minute).padStart(2,'0');"
           "html+='<tr style=\"border-bottom:1px solid #ddd;\"><td style=\"padding:8px;\">'+s.zone+'</td><td style=\"padding:8px;\">'+time+'</td><td style=\"padding:8px;\">'+s.duration+' min</td><td style=\"padding:8px;\">'+days+'</td><td style=\"padding:8px;\">'+type+'</td></tr>';});"
           "html+='</tbody></table>';document.getElementById('scheduleInfo').innerHTML=html;}).catch(err=>{"
           "document.getElementById('scheduleInfo').innerHTML='<div style=\"padding:10px;color:red;\">Error loading schedules</div>';console.error('Error:',err);});}"
           "document.addEventListener('DOMContentLoaded',function(){"
           "initZones();updateTime();updateSystemInfo();updateScheduleInfo();"
           "setInterval(updateTime,10000);setInterval(updateSystemInfo,30000);setInterval(updateScheduleInfo,60000);});"
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
ScheduleManager* HunterWebServer::scheduleManager = nullptr;
EventLogger* HunterWebServer::eventLogger = nullptr;
HTTPScheduleClient* HunterWebServer::httpClient = nullptr;
MQTTManager* HunterWebServer::mqttManager = nullptr;
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

    // Schedule API endpoints
    server.on("/api/schedules", HTTP_GET, handleGetSchedules);
    server.on("/api/schedules", HTTP_POST, handleCreateSchedule);
    server.on("/api/schedules/active", HTTP_GET, handleGetActiveZones);
    server.on("/api/schedules/ai", HTTP_POST, handleSetAISchedules);
    server.on("/api/schedules/ai", HTTP_DELETE, handleClearAISchedules);
    server.on("/api/schedules/fetch", HTTP_POST, handleFetchSchedules);

    // Device status and control endpoints for Node-RED
    server.on("/api/device/status", HTTP_GET, handleGetDeviceStatus);
    server.on("/api/device/next", HTTP_GET, handleGetNextEvent);
    server.on("/api/device/command", HTTP_POST, handleDeviceCommand);

    // MQTT configuration endpoints
    server.on("/api/mqtt/config", HTTP_GET, handleGetMQTTConfig);
    server.on("/api/mqtt/config", HTTP_POST, handleSetMQTTConfig);

    // Event logging endpoints
    server.on("/api/events", HTTP_GET, handleGetEvents);
    server.on("/api/events", HTTP_DELETE, handleClearEvents);
    server.on("/api/events/stats", HTTP_GET, handleGetEventStats);

    // CORS handler for OPTIONS requests
    server.on("/api/events", HTTP_OPTIONS, []() {
        if (serverInstance) {
            serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
            serverInstance->server.sendHeader("Access-Control-Allow-Methods", "GET, DELETE, OPTIONS");
            serverInstance->server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            serverInstance->server.send(204);
        }
    });

    server.on("/api/events/stats", HTTP_OPTIONS, []() {
        if (serverInstance) {
            serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
            serverInstance->server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
            serverInstance->server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            serverInstance->server.send(204);
        }
    });

    server.on("/api/schedules/fetch", HTTP_OPTIONS, []() {
        if (serverInstance) {
            serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
            serverInstance->server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
            serverInstance->server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            serverInstance->server.send(204);
        }
    });

    // 404 handler
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Irrigation ESP32 WebServer started with REST API");
    Serial.print("Build Number: ");
    Serial.println(BUILD_NUMBER);
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
    Serial.println("  GET  /api/schedules       - Get all schedules");
    Serial.println("  POST /api/schedules       - Create new schedule");
    Serial.println("  GET  /api/schedules/active - Get active zones status");
    Serial.println("  POST /api/schedules/ai    - Set AI schedules from Node-RED");
    Serial.println("  DELETE /api/schedules/ai  - Clear AI schedules");
    Serial.println("  POST /api/schedules/fetch - Fetch schedules from server");
    Serial.println("  GET  /api/events          - Get watering event logs");
    Serial.println("  DELETE /api/events        - Clear event logs");
    Serial.println("  GET  /api/events/stats    - Get event statistics");
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

    // Use ScheduleManager if available, otherwise fallback to old method
    if (scheduleManager) {
        // Log manual event start
        uint32_t eventId = 0;
        if (eventLogger) {
            eventId = eventLogger->logEventStart(zoneNum, timeMin, EventType::MANUAL, 0);
        }

        auto result = scheduleManager->startZoneManual(zoneNum, timeMin);

        if (result.hasConflict && result.stoppedZone == 0) {
            String jsonError = "{\"status\":\"error\",\"message\":\"" + result.message + "\"}";
            serverInstance->server.send(409, "application/json", jsonError);
            return;
        }

        String message = "Zone " + String(zoneNum) + " started for " + String(timeMin) + " minutes";
        if (result.hasConflict && result.stoppedZone > 0) {
            message += " (stopped zone " + String(result.stoppedZone) + " to resolve conflict)";
        }
        if (eventId > 0) {
            message += " [Event ID: " + String(eventId) + "]";
        }

        String jsonResponse = "{\"status\":\"success\",\"message\":\"" + message + "\",\"zone\":" + String(zoneNum) + ",\"duration_minutes\":" + String(timeMin);
        if (result.stoppedZone > 0) {
            jsonResponse += ",\"stopped_zone\":" + String(result.stoppedZone);
        }
        if (eventId > 0) {
            jsonResponse += ",\"event_id\":" + String(eventId);
        }
        jsonResponse += "}";

        serverInstance->server.send(200, "application/json", jsonResponse);
        Serial.println("API: " + message);
        return;
    }

    // Fallback to old method if ScheduleManager not available
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

    // Use ScheduleManager if available, otherwise fallback to old method
    if (scheduleManager) {
        bool success = scheduleManager->stopZone(zoneNum);

        if (success) {
            // Log event completion (interrupted since stopped manually)
            if (eventLogger) {
                eventLogger->logEventEnd(0, false); // false = interrupted
            }

            String jsonResponse = "{\"status\":\"success\",\"message\":\"Zone " + String(zoneNum) + " stopped\",\"zone\":" + String(zoneNum) + "}";
            serverInstance->server.send(200, "application/json", jsonResponse);
            Serial.println("API: Zone " + String(zoneNum) + " stopped");
        } else {
            String jsonError = "{\"status\":\"error\",\"message\":\"Zone " + String(zoneNum) + " was not running\"}";
            serverInstance->server.send(404, "application/json", jsonError);
        }
        return;
    }

    // Fallback to old method if ScheduleManager not available
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
    jsonResponse += "\"build_number\":" + String(BUILD_NUMBER) + ",";
    if (configManager) {
        jsonResponse += "\"device_id\":\"" + configManager->getDeviceId() + "\",";
    }
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
    jsonResponse += "},";

    // MQTT status
    jsonResponse += "\"mqtt\":{";
    if (mqttManager && configManager) {
        bool connected = mqttManager->isClientConnected();
        jsonResponse += "\"status\":\"" + String(connected ? "connected" : "disconnected") + "\",";
        unsigned long lastPublish = mqttManager->getLastPublishTime();
        if (lastPublish > 0) {
            unsigned long secondsAgo = (millis() - lastPublish) / 1000;
            jsonResponse += "\"last_device_config_sent\":\"" + String(secondsAgo) + " seconds ago\",";
        } else {
            jsonResponse += "\"last_device_config_sent\":\"never\",";
        }
        jsonResponse += "\"mqtt_enabled\":" + String(configManager->isMQTTEnabled() ? "true" : "false") + ",";
        jsonResponse += "\"mqtt_broker\":\"" + configManager->getMQTTBroker() + "\",";
        jsonResponse += "\"mqtt_port\":" + String(configManager->getMQTTPort()) + ",";
        jsonResponse += "\"mqtt_topic_prefix\":\"" + configManager->getMQTTTopicPrefix() + "\"";
    } else {
        jsonResponse += "\"status\":\"not_configured\"";
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

    // Check if request has JSON body (check both header and presence of body)
    bool hasJsonBody = false;
    if (serverInstance->server.hasArg("plain")) {
        // Has body - check if Content-Type indicates JSON, or assume JSON if body looks like JSON
        String contentType = serverInstance->server.hasHeader("Content-Type") ?
                           serverInstance->server.header("Content-Type") : "";
        hasJsonBody = (contentType.indexOf("application/json") >= 0) ||
                      (serverInstance->server.arg("plain").startsWith("{"));
    }

    Serial.println("API: Config update request");
    Serial.println("  Has JSON body: " + String(hasJsonBody ? "yes" : "no"));
    Serial.println("  Has 'plain' arg: " + String(serverInstance->server.hasArg("plain") ? "yes" : "no"));

    // Parse JSON once if present
    JsonDocument doc;
    DeserializationError jsonError;
    if (hasJsonBody && serverInstance->server.hasArg("plain")) {
        String body = serverInstance->server.arg("plain");
        Serial.println("  Body length: " + String(body.length()));
        Serial.println("  Body: " + body);
        jsonError = deserializeJson(doc, body);
        if (jsonError) {
            Serial.println("  JSON parse error: " + String(jsonError.c_str()));
        } else {
            Serial.println("  JSON parsed successfully");
        }
    }

    // Parse URL parameters or JSON body for configuration updates
    String response = "Configuration updated:\n";
    bool configChanged = false;

    // Helper lambda to get parameter from URL or JSON
    auto getParam = [&](const char* key) -> String {
        if (hasJsonBody && !jsonError) {
            // Use parsed JSON document
            if (doc[key].is<const char*>()) {
                return String(doc[key].as<const char*>());
            } else if (doc[key].is<bool>()) {
                return doc[key].as<bool>() ? "true" : "false";
            } else if (doc[key].is<int>()) {
                return String(doc[key].as<int>());
            }
            return "";
        } else {
            // Parse URL parameters
            return serverInstance->server.hasArg(key) ? serverInstance->server.arg(key) : "";
        }
    };

    // Timezone settings
    String timezoneStr = getParam("timezone");
    if (timezoneStr.length() > 0) {
        float timezoneFloat = timezoneStr.toFloat();
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

    String daylightStr = getParam("daylight_saving");
    if (daylightStr.length() > 0) {
        bool dst = (daylightStr == "true");
        configManager->setDaylightSaving(dst);
        response += "- Daylight Saving: " + String(dst ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    // NTP settings
    String ntpServer1Str = getParam("ntp_server1");
    if (ntpServer1Str.length() > 0) {
        String server2 = getParam("ntp_server2");
        if (server2.length() == 0) server2 = "time.nist.gov";
        configManager->setNTPServers(ntpServer1Str, server2);
        response += "- NTP Servers: " + ntpServer1Str + ", " + server2 + "\n";
        configChanged = true;
    }

    String autoNtpStr = getParam("auto_ntp");
    if (autoNtpStr.length() > 0) {
        bool autoNTP = (autoNtpStr == "true");
        configManager->setAutoNTPSync(autoNTP);
        response += "- Auto NTP Sync: " + String(autoNTP ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    String syncIntervalStr = getParam("sync_interval");
    if (syncIntervalStr.length() > 0) {
        int interval = syncIntervalStr.toInt();
        if (interval > 0 && interval <= 168) {
            configManager->setSyncInterval(interval);
            response += "- Sync Interval: " + String(interval) + " hours\n";
            configChanged = true;
        }
    }

    // MQTT settings
    String mqttEnabledStr = getParam("mqtt_enabled");
    if (mqttEnabledStr.length() > 0) {
        bool enabled = (mqttEnabledStr == "true");
        configManager->setMQTTEnabled(enabled);
        response += "- MQTT Enabled: " + String(enabled ? "Yes" : "No") + "\n";
        configChanged = true;
    }

    String mqttBrokerStr = getParam("mqtt_broker");
    if (mqttBrokerStr.length() > 0) {
        configManager->setMQTTBroker(mqttBrokerStr);
        response += "- MQTT Broker: " + mqttBrokerStr + "\n";
        configChanged = true;
    }

    String mqttPortStr = getParam("mqtt_port");
    if (mqttPortStr.length() > 0) {
        int port = mqttPortStr.toInt();
        if (port > 0 && port <= 65535) {
            configManager->setMQTTPort(port);
            response += "- MQTT Port: " + String(port) + "\n";
            configChanged = true;
        }
    }

    String mqttUsernameStr = getParam("mqtt_username");
    if (mqttUsernameStr.length() > 0) {
        configManager->setMQTTUsername(mqttUsernameStr);
        response += "- MQTT Username: " + mqttUsernameStr + "\n";
        configChanged = true;
    }

    String mqttPasswordStr = getParam("mqtt_password");
    if (mqttPasswordStr.length() > 0) {
        configManager->setMQTTPassword(mqttPasswordStr);
        response += "- MQTT Password: ***\n";
        configChanged = true;
    }

    String mqttTopicPrefixStr = getParam("mqtt_topic_prefix");
    if (mqttTopicPrefixStr.length() > 0) {
        configManager->setMQTTTopicPrefix(mqttTopicPrefixStr);
        response += "- MQTT Topic Prefix: " + mqttTopicPrefixStr + "\n";
        configChanged = true;
    }

    String mqttRetainStr = getParam("mqtt_retain");
    if (mqttRetainStr.length() > 0) {
        bool retain = (mqttRetainStr == "true");
        configManager->setMQTTRetainMessages(retain);
        response += "- MQTT Retain Messages: " + String(retain ? "Yes" : "No") + "\n";
        configChanged = true;
    }

    String mqttKeepAliveStr = getParam("mqtt_keep_alive");
    if (mqttKeepAliveStr.length() > 0) {
        int keepAlive = mqttKeepAliveStr.toInt();
        if (keepAlive > 0 && keepAlive <= 300) {
            configManager->setMQTTKeepAlive(keepAlive);
            response += "- MQTT Keep Alive: " + String(keepAlive) + " seconds\n";
            configChanged = true;
        }
    }

    // Server settings
    String serverEnabledStr = getParam("server_enabled");
    if (serverEnabledStr.length() > 0) {
        bool enabled = (serverEnabledStr == "true");
        configManager->setServerEnabled(enabled);
        response += "- Server Enabled: " + String(enabled ? "Yes" : "No") + "\n";
        configChanged = true;
    }

    String serverUrlStr = getParam("server_url");
    if (serverUrlStr.length() > 0) {
        configManager->setServerUrl(serverUrlStr);
        response += "- Server URL: " + serverUrlStr + "\n";
        configChanged = true;
    }

    String deviceIdStr = getParam("device_id");
    if (deviceIdStr.length() > 0) {
        configManager->setDeviceId(deviceIdStr);
        response += "- Device ID: " + deviceIdStr + "\n";
        configChanged = true;
    }

    String serverRetryIntervalStr = getParam("server_retry_interval");
    if (serverRetryIntervalStr.length() > 0) {
        int interval = serverRetryIntervalStr.toInt();
        if (interval > 0 && interval <= 86400) {
            configManager->setServerRetryInterval(interval);
            response += "- Server Retry Interval: " + String(interval) + " seconds\n";
            configChanged = true;
        }
    }

    String serverMaxRetriesStr = getParam("server_max_retries");
    if (serverMaxRetriesStr.length() > 0) {
        int retries = serverMaxRetriesStr.toInt();
        if (retries > 0 && retries <= 100) {
            configManager->setServerMaxRetries(retries);
            response += "- Server Max Retries: " + String(retries) + "\n";
            configChanged = true;
        }
    }

    String scheduleFetchHourStr = getParam("schedule_fetch_hour");
    if (scheduleFetchHourStr.length() > 0) {
        int hour = scheduleFetchHourStr.toInt();
        if (hour >= 0 && hour <= 23) {
            configManager->setScheduleFetchHour(hour);
            response += "- Schedule Fetch Hour: " + String(hour) + "\n";
            configChanged = true;
        }
    }

    String scheduleFetchMinuteStr = getParam("schedule_fetch_minute");
    if (scheduleFetchMinuteStr.length() > 0) {
        int minute = scheduleFetchMinuteStr.toInt();
        if (minute >= 0 && minute <= 59) {
            configManager->setScheduleFetchMinute(minute);
            response += "- Schedule Fetch Minute: " + String(minute) + "\n";
            configChanged = true;
        }
    }

    String scheduleFetchDaysStr = getParam("schedule_fetch_days");
    if (scheduleFetchDaysStr.length() > 0) {
        int days = scheduleFetchDaysStr.toInt();
        if (days >= 1 && days <= 5) {
            configManager->setScheduleFetchDays(days);
            response += "- Schedule Fetch Days: " + String(days) + "\n";
            configChanged = true;
        }
    }

    // Irrigation settings
    String schedulingStr = getParam("scheduling");
    if (schedulingStr.length() > 0) {
        bool enabled = (schedulingStr == "true");
        configManager->setSchedulingEnabled(enabled);
        response += "- Scheduling: " + String(enabled ? "Enabled" : "Disabled") + "\n";
        configChanged = true;
    }

    String maxRuntimeStr = getParam("max_runtime");
    if (maxRuntimeStr.length() > 0) {
        int maxRuntime = maxRuntimeStr.toInt();
        if (maxRuntime > 0 && maxRuntime <= 1440) {
            configManager->setMaxZoneRunTime(maxRuntime);
            response += "- Max Zone Runtime: " + String(maxRuntime) + " minutes\n";
            configChanged = true;
        }
    }

    String maxZonesStr = getParam("max_enabled_zones");
    if (maxZonesStr.length() > 0) {
        int maxZones = maxZonesStr.toInt();
        if (maxZones >= 1 && maxZones <= 16) {
            configManager->setMaxEnabledZones(maxZones);
            response += "- Max Enabled Zones: " + String(maxZones) + "\n";
            configChanged = true;
        }
    }

    String pumpSafetyStr = getParam("pump_safety");
    if (pumpSafetyStr.length() > 0) {
        bool pumpSafety = (pumpSafetyStr == "true");
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

// ===== SCHEDULE API ENDPOINTS =====

void HunterWebServer::handleGetSchedules() {
    if (!serverInstance) return;

    // Add CORS headers
    serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
    serverInstance->server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    serverInstance->server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    String jsonResponse = scheduleManager->getSchedulesJSON();
    serverInstance->server.send(200, "application/json", jsonResponse);
}

void HunterWebServer::handleCreateSchedule() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    // Parse URL parameters for basic schedule creation
    if (!serverInstance->server.hasArg("zone") || !serverInstance->server.hasArg("hour") ||
        !serverInstance->server.hasArg("minute") || !serverInstance->server.hasArg("duration")) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Missing required parameters: zone, hour, minute, duration\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    uint8_t zone = serverInstance->server.arg("zone").toInt();
    uint8_t hour = serverInstance->server.arg("hour").toInt();
    uint8_t minute = serverInstance->server.arg("minute").toInt();
    uint16_t duration = serverInstance->server.arg("duration").toInt();
    uint8_t dayMask = serverInstance->server.hasArg("days") ? serverInstance->server.arg("days").toInt() : 0b1111111; // Default: every day

    if (zone < 1 || zone > 16 || hour > 23 || minute > 59 || duration < 1 || duration > 1440) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Invalid parameter values\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    uint8_t scheduleId = scheduleManager->addBasicSchedule(zone, dayMask, hour, minute, duration);

    if (scheduleId > 0) {
        String jsonResponse = "{\"status\":\"success\",\"message\":\"Schedule created\",\"schedule_id\":" + String(scheduleId) + "}";
        serverInstance->server.send(201, "application/json", jsonResponse);
    } else {
        String jsonError = "{\"status\":\"error\",\"message\":\"Failed to create schedule\"}";
        serverInstance->server.send(500, "application/json", jsonError);
    }
}

void HunterWebServer::handleGetActiveZones() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    String jsonResponse = scheduleManager->getActiveZonesJSON();
    serverInstance->server.send(200, "application/json", jsonResponse);
}

void HunterWebServer::handleSetAISchedules() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    // Get JSON body from Node-RED
    String body = serverInstance->server.arg("plain");

    if (body.length() == 0) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Empty request body\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    // Process AI schedules batch
    bool success = scheduleManager->setAIScheduleBatch(body);

    if (success) {
        String jsonResponse = "{\"status\":\"success\",\"message\":\"AI schedules updated\"}";
        serverInstance->server.send(200, "application/json", jsonResponse);
        Serial.println("API: AI schedules updated from Node-RED");
    } else {
        String jsonError = "{\"status\":\"error\",\"message\":\"Failed to process AI schedules\"}";
        serverInstance->server.send(500, "application/json", jsonError);
    }
}

void HunterWebServer::handleClearAISchedules() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    scheduleManager->clearAISchedules();
    String jsonResponse = "{\"status\":\"success\",\"message\":\"AI schedules cleared\"}";
    serverInstance->server.send(200, "application/json", jsonResponse);
    Serial.println("API: AI schedules cleared");
}

void HunterWebServer::handleUpdateSchedule() {
    // Implementation for updating individual schedules
    String jsonError = "{\"status\":\"error\",\"message\":\"Update schedule endpoint not implemented yet\"}";
    serverInstance->server.send(501, "application/json", jsonError);
}

void HunterWebServer::handleDeleteSchedule() {
    // Implementation for deleting individual schedules
    String jsonError = "{\"status\":\"error\",\"message\":\"Delete schedule endpoint not implemented yet\"}";
    serverInstance->server.send(501, "application/json", jsonError);
}

// Device status and control handlers for Node-RED interface
void HunterWebServer::handleGetDeviceStatus() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    String deviceStatus = scheduleManager->getDeviceStatusJSON();
    serverInstance->server.send(200, "application/json", deviceStatus);
    Serial.println("API: Device status requested");
}

void HunterWebServer::handleGetNextEvent() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    String nextEvent = scheduleManager->getNextEventJSON();
    serverInstance->server.send(200, "application/json", nextEvent);
    Serial.println("API: Next event requested");
}

void HunterWebServer::handleDeviceCommand() {
    if (!serverInstance) return;

    if (!scheduleManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    if (serverInstance->server.method() != HTTP_POST) {
        String jsonError = "{\"status\":\"error\",\"message\":\"POST method required\"}";
        serverInstance->server.send(405, "application/json", jsonError);
        return;
    }

    String command = serverInstance->server.arg("plain");
    if (command.length() == 0) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Empty command body\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    bool success = scheduleManager->updateScheduleFromJSON(command);
    if (success) {
        String jsonResponse = "{\"status\":\"success\",\"message\":\"Command executed\"}";
        serverInstance->server.send(200, "application/json", jsonResponse);
        Serial.println("API: Device command executed");
    } else {
        String jsonError = "{\"status\":\"error\",\"message\":\"Command execution failed\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        Serial.println("API: Device command failed");
    }
}

void HunterWebServer::handleFetchSchedules() {
    if (!serverInstance) return;

    // Add CORS headers
    serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
    serverInstance->server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    serverInstance->server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

    if (!httpClient) {
        String jsonError = "{\"status\":\"error\",\"message\":\"HTTP client not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    if (serverInstance->server.method() != HTTP_POST) {
        String jsonError = "{\"status\":\"error\",\"message\":\"POST method required\"}";
        serverInstance->server.send(405, "application/json", jsonError);
        return;
    }

    // Get optional days parameter (default 5)
    int days = 5;
    if (serverInstance->server.hasArg("days")) {
        days = serverInstance->server.arg("days").toInt();
        if (days < 1 || days > 5) {
            days = 5;
        }
    }

    Serial.println("API: Manual schedule fetch triggered (" + String(days) + " days)");

    // Trigger the fetch in the background
    bool success = httpClient->fetchSchedule(days);

    if (success) {
        String jsonResponse = "{\"status\":\"success\",\"message\":\"Schedules fetched successfully\",\"days\":" + String(days) + "}";
        serverInstance->server.send(200, "application/json", jsonResponse);
        Serial.println("API: Schedule fetch completed successfully");
    } else {
        String error = httpClient->getLastError();
        String jsonError = "{\"status\":\"error\",\"message\":\"Schedule fetch failed\",\"error\":\"" + error + "\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        Serial.println("API: Schedule fetch failed - " + error);
    }
}

// MQTT configuration handlers
void HunterWebServer::handleGetMQTTConfig() {
    if (!serverInstance) return;

    if (!configManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Config manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    JsonDocument doc;
    doc["mqtt_enabled"] = configManager->isMQTTEnabled();
    doc["mqtt_broker"] = configManager->getMQTTBroker();
    doc["mqtt_port"] = configManager->getMQTTPort();
    doc["mqtt_username"] = configManager->getMQTTUsername();
    doc["mqtt_topic_prefix"] = configManager->getMQTTTopicPrefix();
    doc["mqtt_retain"] = configManager->isMQTTRetainMessages();
    doc["mqtt_keep_alive"] = configManager->getMQTTKeepAlive();

    String jsonResponse;
    serializeJson(doc, jsonResponse);

    serverInstance->server.send(200, "application/json", jsonResponse);
    Serial.println("API: MQTT config requested");
}

void HunterWebServer::handleSetMQTTConfig() {
    if (!serverInstance) return;

    if (!configManager) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Config manager not available\"}";
        serverInstance->server.send(500, "application/json", jsonError);
        return;
    }

    if (serverInstance->server.method() != HTTP_POST) {
        String jsonError = "{\"status\":\"error\",\"message\":\"POST method required\"}";
        serverInstance->server.send(405, "application/json", jsonError);
        return;
    }

    String body = serverInstance->server.arg("plain");
    if (body.length() == 0) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Empty request body\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        String jsonError = "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";
        serverInstance->server.send(400, "application/json", jsonError);
        return;
    }

    // Update MQTT configuration
    if (!doc["mqtt_enabled"].isNull()) {
        configManager->setMQTTEnabled(doc["mqtt_enabled"]);
    }
    if (!doc["mqtt_broker"].isNull()) {
        configManager->setMQTTBroker(doc["mqtt_broker"]);
    }
    if (!doc["mqtt_port"].isNull()) {
        configManager->setMQTTPort(doc["mqtt_port"]);
    }
    if (!doc["mqtt_username"].isNull()) {
        configManager->setMQTTUsername(doc["mqtt_username"]);
    }
    if (!doc["mqtt_password"].isNull()) {
        configManager->setMQTTPassword(doc["mqtt_password"]);
    }
    if (!doc["mqtt_topic_prefix"].isNull()) {
        configManager->setMQTTTopicPrefix(doc["mqtt_topic_prefix"]);
    }
    if (!doc["mqtt_retain"].isNull()) {
        configManager->setMQTTRetainMessages(doc["mqtt_retain"]);
    }
    if (!doc["mqtt_keep_alive"].isNull()) {
        configManager->setMQTTKeepAlive(doc["mqtt_keep_alive"]);
    }
    if (!doc["timezone"].isNull()) {
        float tz = doc["timezone"];
        int halfHours = (int)(tz * 2);
        configManager->setTimezoneOffset(halfHours);
    }

    String jsonResponse = "{\"status\":\"success\",\"message\":\"MQTT configuration updated\"}";
    serverInstance->server.send(200, "application/json", jsonResponse);
    Serial.println("API: MQTT configuration updated");
}

// Event logging handlers
void HunterWebServer::handleGetEvents() {
    if (!serverInstance || !eventLogger) {
        String response = "{\"error\":\"Event logger not initialized\"}";
        if (serverInstance) {
            serverInstance->server.send(500, "application/json", response);
        }
        return;
    }

    // Parse query parameters
    int limit = 100;
    time_t startDate = 0;
    time_t endDate = 0;

    if (serverInstance->server.hasArg("limit")) {
        limit = serverInstance->server.arg("limit").toInt();
        if (limit < 1) limit = 100;
        if (limit > 1000) limit = 1000;
    }

    if (serverInstance->server.hasArg("start_date")) {
        // Expected format: Unix timestamp
        startDate = serverInstance->server.arg("start_date").toInt();
    }

    if (serverInstance->server.hasArg("end_date")) {
        endDate = serverInstance->server.arg("end_date").toInt();
    }

    String jsonResponse = eventLogger->getEventsJson(limit, startDate, endDate);
    serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
    serverInstance->server.send(200, "application/json", jsonResponse);
    Serial.println("API: Retrieved event logs (limit: " + String(limit) + ")");
}

void HunterWebServer::handleClearEvents() {
    if (!serverInstance || !eventLogger) {
        String response = "{\"error\":\"Event logger not initialized\"}";
        if (serverInstance) {
            serverInstance->server.send(500, "application/json", response);
        }
        return;
    }

    // Check if we should clear all or just old events
    bool clearAll = false;
    int daysToKeep = 365;

    if (serverInstance->server.hasArg("all")) {
        String allParam = serverInstance->server.arg("all");
        clearAll = (allParam == "true" || allParam == "1");
    }

    if (serverInstance->server.hasArg("days")) {
        daysToKeep = serverInstance->server.arg("days").toInt();
        if (daysToKeep < 1) daysToKeep = 365;
    }

    JsonDocument doc;

    if (clearAll) {
        bool success = eventLogger->clearAllEvents();
        doc["status"] = success ? "success" : "error";
        doc["message"] = success ? "All events cleared" : "Failed to clear events";
        doc["cleared"] = success ? "all" : 0;
    } else {
        int cleared = eventLogger->clearOldEvents(daysToKeep);
        doc["status"] = "success";
        doc["message"] = "Old events cleared";
        doc["cleared"] = cleared;
        doc["kept_days"] = daysToKeep;
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
    serverInstance->server.send(200, "application/json", jsonResponse);

    Serial.println("API: Cleared events (all=" + String(clearAll) + ", days=" + String(daysToKeep) + ")");
}

void HunterWebServer::handleGetEventStats() {
    if (!serverInstance || !eventLogger) {
        String response = "{\"error\":\"Event logger not initialized\"}";
        if (serverInstance) {
            serverInstance->server.send(500, "application/json", response);
        }
        return;
    }

    time_t startDate = 0;
    time_t endDate = 0;

    if (serverInstance->server.hasArg("start_date")) {
        startDate = serverInstance->server.arg("start_date").toInt();
    }

    if (serverInstance->server.hasArg("end_date")) {
        endDate = serverInstance->server.arg("end_date").toInt();
    }

    String jsonResponse = eventLogger->getStatistics(startDate, endDate);
    serverInstance->server.sendHeader("Access-Control-Allow-Origin", "*");
    serverInstance->server.send(200, "application/json", jsonResponse);
    Serial.println("API: Retrieved event statistics");
}
