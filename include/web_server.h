#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Forward declarations
class RTCModule;
class ConfigManager;
class ScheduleManager;

// Zone schedule structure
struct ZoneSchedule {
    int zoneNumber;
    int hour;
    int minute;
    int duration; // in minutes
    bool enabled;
    bool isActive;
    unsigned long startTime; // millis() when started
};

// Web server class for Hunter ESP32 control
class HunterWebServer {
private:
    WebServer server;

    // Global variables for communication between web handlers and main loop
    static int globalZoneID;
    static int globalProgramID;
    static int globalTime;
    static int globalFlag;

    // Scheduling system
    static ZoneSchedule schedules[16]; // Support for 16 zones
    static int activeZones[16]; // Track which zones are currently running
    static unsigned long zoneStartTimes[16]; // Track when each zone started
    static int zoneDurations[16]; // Track duration for each zone

    // Pin definitions
    static const int PUMP_PIN;

    // RTC module reference
    static RTCModule* rtcModule;

    // Configuration manager reference
    static class ConfigManager* configManager;

    // Schedule manager reference
    static class ScheduleManager* scheduleManager;

    // Private methods for handling requests
    static void handleRoot();
    static void handleNotFound();

    // REST API handlers
    static void handleStartZone();
    static void handleStopZone();
    static void handleRunProgram();
    static void handleGetTime();
    static void handleGetStatus();
    static void handleSetTime();
    static void handleSyncNTP();
    static void handleGetConfig();
    static void handleSetConfig();

    // Schedule API endpoints
    static void handleGetSchedules();
    static void handleCreateSchedule();
    static void handleUpdateSchedule();
    static void handleDeleteSchedule();
    static void handleGetActiveZones();
    static void handleSetAISchedules();
    static void handleClearAISchedules();

    // Device status and control handlers for Node-RED
    static void handleGetDeviceStatus();
    static void handleGetNextEvent();
    static void handleDeviceCommand();

    // MQTT configuration handlers
    static void handleGetMQTTConfig();
    static void handleSetMQTTConfig();

public:
    // Constructor
    HunterWebServer(int port = 80);

    // Initialize the web server
    void begin();

    // Set RTC module reference
    void setRTCModule(RTCModule* rtc) { rtcModule = rtc; }

    // Set Configuration manager reference
    void setConfigManager(class ConfigManager* config) { configManager = config; }

    // Set Schedule manager reference
    void setScheduleManager(class ScheduleManager* scheduler) { scheduleManager = scheduler; }

    // Process any pending commands (call this in main loop)
    void processCommands();

    // Zone timer management
    void checkZoneTimers();
    void startZoneTimer(int zone, int duration);
    void stopZoneTimer(int zone);
    bool isZoneActive(int zone);

    // Scheduling system
    void addSchedule(int zone, int hour, int minute, int duration);
    void removeSchedule(int zone);
    void checkSchedules();
    void enableSchedule(int zone, bool enabled);

    // Getters for global variables
    static int getZoneID() { return globalZoneID; }
    static int getProgramID() { return globalProgramID; }
    static int getTime() { return globalTime; }
    static int getFlag() { return globalFlag; }

    // Reset flag after processing
    static void clearFlag() { globalFlag = 0; }
};

#endif // WEB_SERVER_H