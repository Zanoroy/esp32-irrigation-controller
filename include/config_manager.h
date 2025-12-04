#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Forward declaration
class RTCModule;

// Configuration structure
struct SystemConfig {
    // Timezone settings
    int timezoneOffset;     // Timezone offset in half-hours from UTC (-24 to +28, where 19 = +9:30)
    bool daylightSaving;    // Enable daylight saving time

    // WiFi settings (optional - can override defaults)
    char wifiSSID[32];
    char wifiPassword[64];

    // NTP settings
    char ntpServer1[64];
    char ntpServer2[64];
    bool autoNTPSync;
    int syncInterval;       // Hours between automatic NTP syncs

    // MQTT settings
    char mqttBroker[64];    // MQTT broker IP or hostname
    int mqttPort;           // MQTT broker port (default 1883)
    char mqttUsername[32];  // MQTT username (optional)
    char mqttPassword[64];  // MQTT password (optional)
    char mqttTopicPrefix[32]; // Topic prefix (default "irrigation/")
    bool mqttEnabled;       // Enable MQTT communication
    bool mqttRetainMessages; // Retain MQTT messages
    int mqttKeepAlive;      // MQTT keep-alive interval in seconds

    // HTTP Server settings (for schedule fetching)
    char serverUrl[128];    // Server URL (e.g., "http://172.17.254.10:2880")
    char deviceId[32];      // Device identifier (e.g., "esp32_irrigation_01")
    int serverRetryInterval; // Retry interval in seconds (default 3600 = 1 hour)
    int serverMaxRetries;   // Maximum retry attempts per day (default 24)
    bool serverEnabled;     // Enable server communication

    // Irrigation settings
    bool enableScheduling;
    int maxZoneRunTime;     // Maximum run time in minutes
    int maxEnabledZones;    // Maximum number of enabled zones (1-16)
    bool pumpSafetyMode;    // Turn off pump when no zones active

    // System settings
    uint32_t configVersion; // For future upgrades
    uint32_t checksum;      // Data integrity check
};

class ConfigManager {
private:
    SystemConfig config;
    RTCModule* rtcModule;
    Preferences preferences; // ESP32 NVS storage
    bool configLoaded;

    // Default values
    void setDefaults();
    uint32_t calculateChecksum() const;
    bool validateConfig() const;

    // Storage methods (try EEPROM first, fallback to NVS)
    bool saveToEEPROM();
    bool loadFromEEPROM();
    bool saveToNVS();
    bool loadFromNVS();

public:
    ConfigManager();

    // Initialize configuration system
    bool begin(RTCModule* rtc = nullptr);

    // Configuration management
    bool loadConfig();
    bool saveConfig();
    void resetToDefaults();

    // Timezone methods
    int getTimezoneOffset() const { return config.timezoneOffset; }
    void setTimezoneOffset(int offset);
    bool isDaylightSaving() const { return config.daylightSaving; }
    void setDaylightSaving(bool enabled);
    String getLocalTimeString();

    // WiFi configuration
    String getWiFiSSID() const { return String(config.wifiSSID); }
    String getWiFiPassword() const { return String(config.wifiPassword); }
    void setWiFiCredentials(const String& ssid, const String& password);

    // NTP configuration
    String getNTPServer1() const { return String(config.ntpServer1); }
    String getNTPServer2() const { return String(config.ntpServer2); }
    void setNTPServers(const String& server1, const String& server2);
    bool isAutoNTPSync() const { return config.autoNTPSync; }
    void setAutoNTPSync(bool enabled);
    int getSyncInterval() const { return config.syncInterval; }
    void setSyncInterval(int hours);

    // Irrigation configuration
    bool isSchedulingEnabled() const { return config.enableScheduling; }
    void setSchedulingEnabled(bool enabled);
    int getMaxZoneRunTime() const { return config.maxZoneRunTime; }
    void setMaxZoneRunTime(int minutes);
    int getMaxEnabledZones() const { return config.maxEnabledZones; }
    void setMaxEnabledZones(int zones);
    bool isPumpSafetyMode() const { return config.pumpSafetyMode; }
    void setPumpSafetyMode(bool enabled);

    // Zone validation
    bool isZoneEnabled(int zone) const;

    // MQTT configuration
    bool isMQTTEnabled() const { return config.mqttEnabled; }
    void setMQTTEnabled(bool enabled);
    String getMQTTBroker() const { return String(config.mqttBroker); }
    void setMQTTBroker(const String& broker);
    int getMQTTPort() const { return config.mqttPort; }
    void setMQTTPort(int port);
    String getMQTTUsername() const { return String(config.mqttUsername); }
    void setMQTTUsername(const String& username);
    String getMQTTPassword() const { return String(config.mqttPassword); }
    void setMQTTPassword(const String& password);
    String getMQTTTopicPrefix() const { return String(config.mqttTopicPrefix); }
    void setMQTTTopicPrefix(const String& prefix);
    bool isMQTTRetainMessages() const { return config.mqttRetainMessages; }
    void setMQTTRetainMessages(bool retain);
    int getMQTTKeepAlive() const { return config.mqttKeepAlive; }
    void setMQTTKeepAlive(int keepAlive);

    // HTTP Server configuration
    bool isServerEnabled() const { return config.serverEnabled; }
    void setServerEnabled(bool enabled);
    String getServerUrl() const { return String(config.serverUrl); }
    void setServerUrl(const String& url);
    String getDeviceId() const { return String(config.deviceId); }
    void setDeviceId(const String& id);
    int getServerRetryInterval() const { return config.serverRetryInterval; }
    void setServerRetryInterval(int seconds);
    int getServerMaxRetries() const { return config.serverMaxRetries; }
    void setServerMaxRetries(int retries);

    // Utility methods
    void printConfig();
    bool isConfigValid() const { return configLoaded && validateConfig(); }
    String getConfigJSON();
    bool setConfigFromJSON(const String& json);
};

#endif // CONFIG_MANAGER_H