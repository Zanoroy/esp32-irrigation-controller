#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

// Forward declarations
class ConfigManager;
class RTCModule;

// Schedule types
enum ScheduleType {
    BASIC = 0,  // Basic schedule stored in NVS
    AI = 1      // AI schedule from Node-RED with expiry
};

// Zone states
enum ZoneState {
    IDLE = 0,
    SCHEDULED = 1,      // Zone is scheduled to run
    RUNNING = 2,        // Zone is currently running
    COMPLETED = 3,      // Zone has completed its run
    RAINDELAYED = 4,    // Zone delayed due to rain
    RAINCANCELLED = 5   // Zone cancelled due to rain
};

// Schedule entry structure
struct ScheduleEntry {
    uint8_t id;             // Unique schedule ID
    uint8_t zone;           // Zone number (1-16)
    uint8_t dayMask;        // Day mask: bit 0=Sunday, bit 6=Saturday
    uint8_t startHour;      // Start hour (0-23)
    uint8_t startMinute;    // Start minute (0-59)
    uint16_t duration;      // Duration in minutes
    bool enabled;           // Schedule enabled flag
    ScheduleType type;      // BASIC or AI schedule
    uint32_t createdTime;   // Unix timestamp when created
    uint32_t expiryTime;    // Unix timestamp when AI schedule expires (0 = never)
};

// Active zone tracking
struct ActiveZone {
    uint8_t zone;           // Zone number
    ZoneState state;        // Current zone state
    uint32_t startTime;     // Millis when started
    uint32_t duration;      // Duration in milliseconds
    bool isScheduled;       // True if started by schedule, false if manual
    uint8_t scheduleId;     // ID of schedule that started this zone
    uint32_t timeRemaining; // Calculated remaining time in seconds
};

// Conflict resolution result
struct ConflictResult {
    bool hasConflict;
    String message;
    uint8_t stoppedZone;    // Zone that was stopped to resolve conflict
};

class ScheduleManager {
private:
    static const uint8_t MAX_SCHEDULES = 48;  // 24 basic + 24 AI schedules
    static const uint8_t MAX_ACTIVE_ZONES = 2; // Maximum concurrent zones

    ScheduleEntry schedules[MAX_SCHEDULES];
    ActiveZone activeZones[MAX_ACTIVE_ZONES];
    uint8_t scheduleCount;
    uint8_t nextScheduleId;

    ConfigManager* configManager;
    RTCModule* rtcModule;

    // Rain control
    bool rainDelayActive;
    uint32_t rainDelayEndTime;      // Unix timestamp when rain delay ends
    bool scheduleEnabled;           // Global schedule enable/disable

    // Schedule management
    bool isScheduleSlotFree(uint8_t index);
    uint8_t findScheduleById(uint8_t id);
    uint8_t findFreeScheduleSlot();
    void cleanupExpiredAISchedules();

    // Zone management
    int8_t findActiveZone(uint8_t zone);
    int8_t findFreeActiveSlot();
    uint8_t getActiveZoneCount();
    ConflictResult resolveZoneConflict(uint8_t newZone, bool isManual);
    uint32_t getRemainingTime(uint8_t activeIndex);

    // Time utilities
    bool isTimeMatch(const ScheduleEntry& schedule, const DateTime& now);
    uint32_t getCurrentUnixTime();

public:
    ScheduleManager();

    // Initialization
    bool begin(ConfigManager* config, RTCModule* rtc);

    // Schedule management
    uint8_t addBasicSchedule(uint8_t zone, uint8_t dayMask, uint8_t hour, uint8_t minute, uint16_t duration);
    uint8_t addAISchedule(uint8_t zone, uint8_t dayMask, uint8_t hour, uint8_t minute, uint16_t duration, uint32_t expiryTime);
    bool removeSchedule(uint8_t id);
    bool enableSchedule(uint8_t id, bool enabled);
    void clearAISchedules();
    void clearAllSchedules();

    // Schedule execution
    void checkAndExecuteSchedules();

    // Manual zone control
    ConflictResult startZoneManual(uint8_t zone, uint16_t duration);
    bool stopZone(uint8_t zone);
    void stopAllZones();

    // Status and information
    String getSchedulesJSON();
    String getActiveZonesJSON();
    String getStatusJSON();
    uint8_t getScheduleCount(ScheduleType type = BASIC);
    bool hasActiveZones();

    // AI schedule management
    bool setAIScheduleBatch(const String& jsonSchedules);
    bool isAIScheduleValid();
    void revertToBasicSchedule();

    // Rain control
    void setRainDelay(uint32_t delayMinutes);
    void cancelZoneForRain(uint8_t zone);
    void clearRainDelay();

    // Status reporting for Node-RED interface
    String getDeviceStatusJSON();       // Full device status
    String getNextEventJSON();          // Next scheduled event
    bool updateScheduleFromJSON(const String& jsonCommand);  // Handle Node-RED commands

    // Process running zones (call from main loop)
    void processActiveZones();

    // Callback function pointer for zone control
    void setZoneControlCallback(void (*callback)(uint8_t zone, bool state, uint16_t duration, ScheduleType schedType, uint8_t schedId));

private:
    void (*zoneControlCallback)(uint8_t zone, bool state, uint16_t duration, ScheduleType schedType, uint8_t schedId) = nullptr;
};

#endif // SCHEDULE_MANAGER_H