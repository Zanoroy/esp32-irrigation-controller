/*
  This project is a web based remote control for a Hunter X-Core Sprinkler controller
  adapted for ESP32 D1 Mini board using PlatformIO.

  Installation with PlatformIO:
  - Open this project folder in VS Code with PlatformIO extension
  - PlatformIO will automatically install all dependencies
  - Update WiFi credentials in this file
  - Use PlatformIO: Upload to flash the ESP32
  - Use PlatformIO: Serial Monitor to view debug output

  Hardware Setup:
  - Connect GPIO 16 of the ESP32 to the REM pin of the X-Core controller
  - Connect a 3.3V pin of the ESP32 to the AC pin next to REM on the X-Core controller
  - Power-up the ESP32 with a floating power-supply

  Web Interface:
  - The ESP32 will display its IP address in the serial monitor
  - Access the web interface at http://[ESP32_IP_ADDRESS]

  Copyright 2021, Claude Loullingen
  Modified 2025 for ESP32 D1 Mini with PlatformIO compatibility
  GNU General Public License v3.0
*/

#include <Arduino.h>
#include <WiFi.h>
// #include "hunter_esp32.h"
#include "HunterRoam.h"
#include "web_server.h"
#include "rtc_module.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "mqtt_manager.h"
#include "http_client.h"

// Define constants
const bool   PUMP_PIN_DEFAULT = false; // Set to true to set PUMP_PIN as On by default
const int    PUMP_PIN = 5; // GPIO5

// WiFi credentials - Can be set here or via build_flags in platformio.ini
#ifndef WIFI_SSID
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#else
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
#endif

// Server URL - Can be set via build_flags in platformio.ini
#ifndef SERVER_URL
const char* serverUrl = "http://172.17.254.10:2880";
#else
const char* serverUrl = SERVER_URL;
#endif

// Create web server, RTC, configuration, schedule manager, MQTT manager, and HTTP client instances
HunterWebServer hunterServer(80);
RTCModule rtcModule;
ConfigManager configManager;
ScheduleManager scheduleManager;
MQTTManager mqttManager;
HTTPScheduleClient httpClient;
HunterRoam hunterController(HUNTER_PIN);
// Zone control callback function for ScheduleManager
void zoneControlCallback(uint8_t zoneNumber, bool enable) {
  Serial.println("Zone control callback: Zone " + String(zoneNumber) + " -> " + (enable ? "ON" : "OFF"));

  if (enable) {
    // Start the zone using Hunter protocol (zone, time in minutes)
    // Use a default of 10 minutes for scheduled zones - this will be overridden by schedule duration
    hunterController.startZone(zoneNumber, 10);
    Serial.println("Zone " + String(zoneNumber) + " started via schedule");
  } else {
    // Stop the zone
    hunterController.stopZone(zoneNumber);
    Serial.println("Zone " + String(zoneNumber) + " stopped via schedule");
  }
}

// Daily schedule fetch task - runs at 6:15 AM
void checkAndFetchDailySchedule() {
  static int lastFetchDay = -1;
  static bool fetchAttemptedToday = false;
  
  if (!rtcModule.isInitialized()) {
    return;
  }
  
  DateTime now = rtcModule.getCurrentTime();
  int currentDay = now.day();
  int currentHour = now.hour();
  int currentMinute = now.minute();
  
  // Reset fetch flag on new day
  if (currentDay != lastFetchDay) {
    fetchAttemptedToday = false;
    lastFetchDay = currentDay;
  }
  
  // Fetch schedule at 6:15 AM if not already attempted today
  if (!fetchAttemptedToday && currentHour == 6 && currentMinute >= 15 && currentMinute < 20) {
    fetchAttemptedToday = true;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ Daily schedule fetch: WiFi not connected, skipping");
      return;
    }
    
    Serial.println("");
    Serial.println("=== DAILY SCHEDULE FETCH ===");
    Serial.println("Time: " + rtcModule.getDateTimeString());
    
    if (httpClient.fetchTodaySchedule()) {
      Serial.println("✅ Daily schedule fetched successfully from server");
      Serial.println("============================");
    } else {
      Serial.println("⚠️ Failed to fetch daily schedule: " + httpClient.getLastError());
      Serial.println("   Using local/cached schedules");
      Serial.println("============================");
    }
  }
}


void setup(void){
  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(1000); // Allow serial to stabilize

  Serial.println("Irrigation ESP32 Controller Starting...");
  Serial.println("Built with PlatformIO");

  // Print system information
  Serial.println("ESP32 System Info:");
  Serial.println("  Chip Model: " + String(ESP.getChipModel()));
  Serial.println("  Chip Revision: " + String(ESP.getChipRevision()));
  Serial.println("  CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println("  Flash Size: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  Serial.println("  Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("  Heap Size: " + String(ESP.getHeapSize()) + " bytes");
  Serial.println("");

  // Configure pins
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(HUNTER_PIN, OUTPUT);

  // Set PUMP_PIN to default value
  digitalWrite(PUMP_PIN, PUMP_PIN_DEFAULT ? HIGH : LOW);
  digitalWrite(HUNTER_PIN, LOW);

  Serial.println("Pump pin: GPIO" + String(PUMP_PIN) + " set to " + (PUMP_PIN_DEFAULT ? "HIGH" : "LOW"));
  Serial.println("Hunter pin: GPIO" + String(HUNTER_PIN) + " initialized");

  // Initialize RTC module
  Serial.println("");
  if (rtcModule.begin()) {
    Serial.println("RTC module initialized successfully");
  } else {
    Serial.println("WARNING: RTC module failed to initialize");
    Serial.println("System will continue without RTC functionality");
  }

  // Initialize Configuration Manager
  Serial.println("");
  Serial.println("Initializing Configuration Manager...");
  if (configManager.begin(&rtcModule)) {
    Serial.println("Configuration Manager initialized successfully");
    configManager.printConfig();
  } else {
    Serial.println("WARNING: Configuration Manager failed to initialize");
    Serial.println("Using default settings");
  }

  // Initialize Schedule Manager
  Serial.println("");
  Serial.println("Initializing Schedule Manager...");
  if (scheduleManager.begin(&configManager, &rtcModule)) {
    Serial.println("Schedule Manager initialized successfully");

    // Set the zone control callback
    scheduleManager.setZoneControlCallback(zoneControlCallback);
    Serial.println("Zone control callback configured");
  } else {
    Serial.println("WARNING: Schedule Manager failed to initialize");
  }

  // Connect to Wi-Fi
  Serial.println("Connecting to WiFi: " + String(ssid));
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());

    // Sync RTC with NTP now that WiFi is connected
    if (rtcModule.isInitialized()) {
      Serial.println("");
      Serial.println("Attempting to sync RTC with NTP time...");
      bool ntpSuccess = false;

      if (configManager.isConfigValid()) {
        // Use configured NTP servers and timezone
        ntpSuccess = rtcModule.syncWithNTP(
          configManager.getNTPServer1().c_str(),
          configManager.getNTPServer2().c_str(),
          configManager.getTimezoneOffset()
        );
        if (ntpSuccess) {
          Serial.println("RTC synchronized with configured NTP settings");
          Serial.println("Local time: " + configManager.getLocalTimeString());
        }
      } else {
        // Fallback to default NTP sync
        ntpSuccess = rtcModule.syncWithNTP();
        if (ntpSuccess) {
          Serial.println("RTC synchronized with default NTP servers");
        }
      }

      if (!ntpSuccess) {
        Serial.println("NTP sync failed, RTC will use current time");
      }
    }
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi!");
    Serial.println("Please check your credentials and try again.");
  }

  // Initialize and start web server
  hunterServer.setRTCModule(&rtcModule);
  hunterServer.setConfigManager(&configManager);
  hunterServer.setScheduleManager(&scheduleManager);
  hunterServer.begin();

  // Initialize MQTT Manager
  Serial.println("");
  Serial.println("Initializing MQTT Manager...");
  if (mqttManager.begin(&configManager, &scheduleManager, &rtcModule)) {
    Serial.println("MQTT Manager initialized successfully");
  } else {
    Serial.println("WARNING: MQTT Manager failed to initialize");
  }

  // Initialize HTTP Schedule Client
  Serial.println("");
  Serial.println("Initializing HTTP Schedule Client...");
  if (httpClient.begin(&configManager, &scheduleManager)) {
    httpClient.setServerUrl(serverUrl);
    Serial.println("HTTP Schedule Client initialized successfully");
    
    // Test connection to server
    if (WiFi.status() == WL_CONNECTED) {
      if (httpClient.testConnection()) {
        Serial.println("✅ Server connection test successful");
        
        // Fetch today's schedule on startup
        Serial.println("Fetching today's schedule from server...");
        if (httpClient.fetchTodaySchedule()) {
          Serial.println("✅ Today's schedule loaded from server");
        } else {
          Serial.println("⚠️ Failed to fetch today's schedule: " + httpClient.getLastError());
          Serial.println("   Will use local schedules or retry later");
        }
      } else {
        Serial.println("⚠️ Server connection test failed: " + httpClient.getLastError());
        Serial.println("   Will use local schedules and retry later");
      }
    }
  } else {
    Serial.println("WARNING: HTTP Schedule Client failed to initialize");
  }

  // Print current time at startup
  if (rtcModule.isInitialized()) {
    Serial.println("");
    Serial.println("=== STARTUP TIME ===");
    Serial.println("Current time: " + rtcModule.getDateTimeString());
    Serial.println("====================");
  }

  Serial.println("=================================");
  Serial.println("Irrigation ESP32 Controller Ready!");
  Serial.println("Access the web interface at: http://" + WiFi.localIP().toString());
  Serial.println("=================================");
}

void loop(void)
{
  static unsigned long lastNTPSync = 0;
  // Get sync interval from configuration (convert hours to milliseconds)
  unsigned long NTP_SYNC_INTERVAL = configManager.isConfigValid() ?
    (configManager.getSyncInterval() * 60UL * 60UL * 1000UL) :
    (24UL * 60UL * 60UL * 1000UL); // Default 24 hours

  // Handle WiFi reconnection if connection is lost
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.begin(ssid, password);
    int reconnectAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && reconnectAttempts < 10) {
      delay(1000);
      Serial.print(".");
      reconnectAttempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi reconnected. IP: " + WiFi.localIP().toString());

      // Sync RTC with NTP after WiFi reconnection using configuration
      if (rtcModule.isInitialized() && configManager.isAutoNTPSync() &&
          millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
        Serial.println("Syncing RTC with NTP after WiFi reconnection...");
        bool ntpSuccess = false;

        if (configManager.isConfigValid()) {
          ntpSuccess = rtcModule.syncWithNTP(
            configManager.getNTPServer1().c_str(),
            configManager.getNTPServer2().c_str(),
            configManager.getTimezoneOffset()
          );
        } else {
          ntpSuccess = rtcModule.syncWithNTP();
        }

        if (ntpSuccess) {
          lastNTPSync = millis();
          Serial.println("RTC synchronized with NTP after reconnection");
        }
      }
    }
  }

  // Periodic NTP synchronization using configuration settings
  if (WiFi.status() == WL_CONNECTED && rtcModule.isInitialized() &&
      configManager.isAutoNTPSync()) {
    if (millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
      Serial.printf("Performing NTP synchronization (interval: %d hours)...\n",
                    configManager.getSyncInterval());

      bool ntpSuccess = false;
      if (configManager.isConfigValid()) {
        ntpSuccess = rtcModule.syncWithNTP(
          configManager.getNTPServer1().c_str(),
          configManager.getNTPServer2().c_str(),
          configManager.getTimezoneOffset()
        );
        if (ntpSuccess) {
          Serial.println("NTP sync completed with configured settings");
          Serial.println("Local time: " + configManager.getLocalTimeString());
        }
      } else {
        ntpSuccess = rtcModule.syncWithNTP();
        if (ntpSuccess) {
          Serial.println("NTP sync completed with default settings");
        }
      }

      if (ntpSuccess) {
        lastNTPSync = millis();
      } else {
        Serial.println("NTP sync failed, will retry later");
      }
    }
  }

  // Process any pending Hunter commands from web server
  hunterServer.processCommands();

  // Process MQTT communication
  mqttManager.loop();

  // Check for daily schedule fetch (runs at 6:15 AM)
  checkAndFetchDailySchedule();

  // Check and execute scheduled zones
  scheduleManager.checkAndExecuteSchedules();

  // Process active zones (handle zone timeouts and cleanup)
  scheduleManager.processActiveZones();

  // Feed the watchdog and yield to other tasks
  yield();

  // Check memory status periodically (every 30 seconds)
  static unsigned long lastMemCheck = 0;
  if (millis() - lastMemCheck > 30000) {
    lastMemCheck = millis();
    if (ESP.getFreeHeap() < 10000) {
      Serial.println("WARNING: Low heap memory: " + String(ESP.getFreeHeap()) + " bytes");
    }
  }

  delay(10);
}