#include "config_manager.h"
#include "rtc_module.h"

// EEPROM addresses for configuration storage
#define CONFIG_START_ADDRESS 0x0000
#define CONFIG_MAGIC_NUMBER 0xC0FF1CE5

ConfigManager::ConfigManager() : rtcModule(nullptr), configLoaded(false) {
    setDefaults();
}

bool ConfigManager::begin(RTCModule* rtc) {
    rtcModule = rtc;

    // Initialize ESP32 NVS (Non-Volatile Storage)
    if (!preferences.begin("hunter_config", false)) {
        Serial.println("Failed to initialize NVS preferences");
        return false;
    }

    Serial.println("Configuration Manager initialized");
    return loadConfig();
}

void ConfigManager::setDefaults() {
    // Timezone defaults (Australia/Adelaide UTC+9:30)
    config.timezoneOffset = 19; // 19 half-hours = UTC+9:30
    config.daylightSaving = false;

    // WiFi defaults (empty - use main.cpp defaults)
    strcpy(config.wifiSSID, "");
    strcpy(config.wifiPassword, "");

    // NTP defaults
    strcpy(config.ntpServer1, "pool.ntp.org");
    strcpy(config.ntpServer2, "time.nist.gov");
    config.autoNTPSync = true;
    config.syncInterval = 24; // 24 hours

    // MQTT defaults
    strcpy(config.mqttBroker, "");
    config.mqttPort = 1883;
    strcpy(config.mqttUsername, "");
    strcpy(config.mqttPassword, "");
    strcpy(config.mqttTopicPrefix, "irrigation/");
    config.mqttEnabled = false;
    config.mqttRetainMessages = true;
    config.mqttKeepAlive = 60; // 60 seconds

    // HTTP Server defaults
    strcpy(config.serverUrl, "http://172.17.254.10:2880");
    strcpy(config.deviceId, "esp32_irrigation_01");
    config.serverRetryInterval = 3600; // 1 hour
    config.serverMaxRetries = 24; // 24 retries per day
    config.serverEnabled = true;

    // Irrigation defaults
    config.enableScheduling = true;
    config.maxZoneRunTime = 240; // 4 hours max
    config.maxEnabledZones = 8;   // Default to 8 zones enabled
    config.pumpSafetyMode = true;

    // System
    config.configVersion = 1;
    config.checksum = 0;

    Serial.println("Configuration reset to defaults");
}

uint32_t ConfigManager::calculateChecksum() const {
    uint32_t checksum = 0;
    const uint8_t* data = (const uint8_t*)&config;

    // Calculate checksum excluding the checksum field itself
    for (size_t i = 0; i < sizeof(SystemConfig) - sizeof(uint32_t); i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 31); // Rotate left
    }

    return checksum;
}

bool ConfigManager::validateConfig() const {
    uint32_t calculatedChecksum = calculateChecksum();
    return (config.checksum == calculatedChecksum &&
            config.configVersion > 0 &&
            config.timezoneOffset >= -24 && config.timezoneOffset <= 28 &&  // Half-hour increments
            config.syncInterval > 0 && config.syncInterval <= 168 && // Max 1 week
            config.maxZoneRunTime > 0 && config.maxZoneRunTime <= 1440 && // Max 24 hours
            config.maxEnabledZones >= 1 && config.maxEnabledZones <= 16); // 1-16 zones
}

bool ConfigManager::loadConfig() {
    bool loaded = false;

    // Try loading from EEPROM first (if available)
    if (rtcModule && rtcModule->isEEPROMAvailable()) {
        Serial.println("Attempting to load config from EEPROM...");
        if (loadFromEEPROM()) {
            Serial.println("Configuration loaded from EEPROM");
            loaded = true;
        } else {
            Serial.println("Failed to load from EEPROM, trying NVS...");
        }
    }

    // Fallback to ESP32 NVS
    if (!loaded) {
        Serial.println("Loading configuration from NVS...");
        if (loadFromNVS()) {
            Serial.println("Configuration loaded from NVS");
            loaded = true;
        } else {
            Serial.println("No valid configuration found, using defaults");
            setDefaults();
        }
    }

    // Validate loaded configuration
    if (loaded && !validateConfig()) {
        Serial.println("Configuration validation failed, using defaults");
        setDefaults();
        loaded = false;
    }

    configLoaded = true;
    return loaded;
}

bool ConfigManager::saveConfig() {
    // Update checksum before saving
    config.checksum = calculateChecksum();

    bool saved = false;

    // Try saving to EEPROM first (if available)
    if (rtcModule && rtcModule->isEEPROMAvailable()) {
        Serial.println("Saving configuration to EEPROM...");
        if (saveToEEPROM()) {
            Serial.println("Configuration saved to EEPROM");
            saved = true;
        } else {
            Serial.println("EEPROM save failed, trying NVS...");
        }
    }

    // Always save to NVS as backup
    Serial.println("Saving configuration to NVS...");
    if (saveToNVS()) {
        Serial.println("Configuration saved to NVS");
        saved = true;
    } else {
        Serial.println("NVS save failed");
    }

    return saved;
}

bool ConfigManager::saveToEEPROM() {
    if (!rtcModule || !rtcModule->isEEPROMAvailable()) return false;

    // Write magic number first
    uint32_t magic = CONFIG_MAGIC_NUMBER;
    if (!rtcModule->writeEEPROM(CONFIG_START_ADDRESS, (uint8_t*)&magic, sizeof(magic))) {
        return false;
    }

    // Write configuration data
    return rtcModule->writeEEPROM(CONFIG_START_ADDRESS + sizeof(magic),
                                  (uint8_t*)&config, sizeof(config));
}

bool ConfigManager::loadFromEEPROM() {
    if (!rtcModule || !rtcModule->isEEPROMAvailable()) return false;

    // Check magic number
    uint32_t magic;
    if (!rtcModule->readEEPROM(CONFIG_START_ADDRESS, (uint8_t*)&magic, sizeof(magic))) {
        return false;
    }

    if (magic != CONFIG_MAGIC_NUMBER) {
        Serial.println("EEPROM magic number mismatch");
        return false;
    }

    // Read configuration data
    return rtcModule->readEEPROM(CONFIG_START_ADDRESS + sizeof(magic),
                                 (uint8_t*)&config, sizeof(config));
}

bool ConfigManager::saveToNVS() {
    preferences.putBytes("config", &config, sizeof(config));
    preferences.putUInt("magic", CONFIG_MAGIC_NUMBER);
    return true;
}

bool ConfigManager::loadFromNVS() {
    uint32_t magic = preferences.getUInt("magic", 0);
    if (magic != CONFIG_MAGIC_NUMBER) {
        Serial.println("NVS magic number mismatch");
        return false;
    }

    size_t configSize = preferences.getBytesLength("config");
    if (configSize != sizeof(config)) {
        Serial.println("NVS config size mismatch");
        return false;
    }

    return (preferences.getBytes("config", &config, sizeof(config)) == sizeof(config));
}

void ConfigManager::resetToDefaults() {
    setDefaults();
    saveConfig();
}

void ConfigManager::setTimezoneOffset(int offset) {
    if (offset >= -24 && offset <= 28) {  // Half-hour increments: -12:00 to +14:00
        config.timezoneOffset = offset;
        int hours = offset / 2;
        int minutes = (offset % 2) * 30;
        Serial.printf("Timezone offset set to %+d:%02d\n", hours, minutes);
    }
}

void ConfigManager::setDaylightSaving(bool enabled) {
    config.daylightSaving = enabled;
    Serial.printf("Daylight saving %s\n", enabled ? "enabled" : "disabled");
}

String ConfigManager::getLocalTimeString() {
    if (!rtcModule || !rtcModule->isRunning()) {
        return "RTC not available";
    }

    DateTime utcTime = rtcModule->getCurrentTime();

    // Apply timezone offset (convert half-hours to seconds)
    long totalOffset = config.timezoneOffset * 1800; // Convert half-hours to seconds

    // Apply daylight saving time (add 1 hour if enabled)
    if (config.daylightSaving) {
        totalOffset += 3600;
    }

    DateTime localTime = DateTime(utcTime.unixtime() + totalOffset);

    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             localTime.year(), localTime.month(), localTime.day(),
             localTime.hour(), localTime.minute(), localTime.second());

    int hours = config.timezoneOffset / 2;
    int minutes = (config.timezoneOffset % 2) * 30;

    char timezoneStr[16];
    snprintf(timezoneStr, sizeof(timezoneStr), "%+d:%02d", hours, minutes);

    return String(timeStr) + " (UTC" + String(timezoneStr) + (config.daylightSaving ? " DST" : "") + ")";
}

void ConfigManager::setWiFiCredentials(const String& ssid, const String& password) {
    strncpy(config.wifiSSID, ssid.c_str(), sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, password.c_str(), sizeof(config.wifiPassword) - 1);
    config.wifiSSID[sizeof(config.wifiSSID) - 1] = '\0';
    config.wifiPassword[sizeof(config.wifiPassword) - 1] = '\0';
}

void ConfigManager::setNTPServers(const String& server1, const String& server2) {
    strncpy(config.ntpServer1, server1.c_str(), sizeof(config.ntpServer1) - 1);
    strncpy(config.ntpServer2, server2.c_str(), sizeof(config.ntpServer2) - 1);
    config.ntpServer1[sizeof(config.ntpServer1) - 1] = '\0';
    config.ntpServer2[sizeof(config.ntpServer2) - 1] = '\0';
}

void ConfigManager::setAutoNTPSync(bool enabled) {
    config.autoNTPSync = enabled;
}

void ConfigManager::setSyncInterval(int hours) {
    if (hours > 0 && hours <= 168) { // 1 hour to 1 week
        config.syncInterval = hours;
    }
}

void ConfigManager::setSchedulingEnabled(bool enabled) {
    config.enableScheduling = enabled;
}

void ConfigManager::setMaxZoneRunTime(int minutes) {
    if (minutes > 0 && minutes <= 1440) { // 1 minute to 24 hours
        config.maxZoneRunTime = minutes;
    }
}

void ConfigManager::setPumpSafetyMode(bool enabled) {
    config.pumpSafetyMode = enabled;
}

void ConfigManager::setMaxEnabledZones(int zones) {
    if (zones >= 1 && zones <= 16) {
        config.maxEnabledZones = zones;
        Serial.printf("Max enabled zones set to %d\n", zones);
    }
}

bool ConfigManager::isZoneEnabled(int zone) const {
    return (zone >= 1 && zone <= config.maxEnabledZones);
}

void ConfigManager::printConfig() {
    Serial.println("=== System Configuration ===");
    Serial.printf("Config Version: %u\n", config.configVersion);
    int hours = config.timezoneOffset / 2;
    int minutes = (config.timezoneOffset % 2) * 30;
    Serial.printf("Timezone: UTC%+d:%02d %s\n", hours, minutes,
                  config.daylightSaving ? "(DST enabled)" : "");
    Serial.printf("Local Time: %s\n", getLocalTimeString().c_str());
    Serial.printf("WiFi SSID: %s\n", strlen(config.wifiSSID) > 0 ? config.wifiSSID : "(using defaults)");
    Serial.printf("NTP Server 1: %s\n", config.ntpServer1);
    Serial.printf("NTP Server 2: %s\n", config.ntpServer2);
    Serial.printf("Auto NTP Sync: %s (every %d hours)\n",
                  config.autoNTPSync ? "Enabled" : "Disabled", config.syncInterval);
    Serial.printf("Scheduling: %s\n", config.enableScheduling ? "Enabled" : "Disabled");
    Serial.printf("Max Zone Runtime: %d minutes\n", config.maxZoneRunTime);
    Serial.printf("Max Enabled Zones: %d\n", config.maxEnabledZones);
    Serial.printf("Pump Safety Mode: %s\n", config.pumpSafetyMode ? "Enabled" : "Disabled");
    Serial.printf("Checksum: 0x%08X\n", config.checksum);
    Serial.println("=============================");
}

String ConfigManager::getConfigJSON() {
    int hours = config.timezoneOffset / 2;
    int minutes = (config.timezoneOffset % 2) * 30;
    float timezoneFloat = hours + (minutes / 60.0);

    String json = "{";
    json += "\"version\":" + String(config.configVersion) + ",";
    json += "\"timezone\":" + String(timezoneFloat, 1) + ",";
    json += "\"daylight_saving\":" + String(config.daylightSaving ? "true" : "false") + ",";
    json += "\"ntp_server1\":\"" + String(config.ntpServer1) + "\",";
    json += "\"ntp_server2\":\"" + String(config.ntpServer2) + "\",";
    json += "\"auto_ntp\":" + String(config.autoNTPSync ? "true" : "false") + ",";
    json += "\"sync_interval\":" + String(config.syncInterval) + ",";
    json += "\"mqtt_enabled\":" + String(config.mqttEnabled ? "true" : "false") + ",";
    json += "\"mqtt_broker\":\"" + String(config.mqttBroker) + "\",";
    json += "\"mqtt_port\":" + String(config.mqttPort) + ",";
    json += "\"mqtt_username\":\"" + String(config.mqttUsername) + "\",";
    json += "\"mqtt_topic_prefix\":\"" + String(config.mqttTopicPrefix) + "\",";
    json += "\"mqtt_retain\":" + String(config.mqttRetainMessages ? "true" : "false") + ",";
    json += "\"mqtt_keep_alive\":" + String(config.mqttKeepAlive) + ",";
    json += "\"server_enabled\":" + String(config.serverEnabled ? "true" : "false") + ",";
    json += "\"server_url\":\"" + String(config.serverUrl) + "\",";
    json += "\"device_id\":\"" + String(config.deviceId) + "\",";
    json += "\"server_retry_interval\":" + String(config.serverRetryInterval) + ",";
    json += "\"server_max_retries\":" + String(config.serverMaxRetries) + ",";
    json += "\"scheduling\":" + String(config.enableScheduling ? "true" : "false") + ",";
    json += "\"max_runtime\":" + String(config.maxZoneRunTime) + ",";
    json += "\"max_enabled_zones\":" + String(config.maxEnabledZones) + ",";
    json += "\"pump_safety\":" + String(config.pumpSafetyMode ? "true" : "false");
    json += "}";
    return json;
}

// MQTT Configuration Methods
void ConfigManager::setMQTTEnabled(bool enabled) {
    config.mqttEnabled = enabled;
    saveConfig();
}

void ConfigManager::setMQTTBroker(const String& broker) {
    strncpy(config.mqttBroker, broker.c_str(), sizeof(config.mqttBroker) - 1);
    config.mqttBroker[sizeof(config.mqttBroker) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setMQTTPort(int port) {
    if (port > 0 && port <= 65535) {
        config.mqttPort = port;
        saveConfig();
    }
}

void ConfigManager::setMQTTUsername(const String& username) {
    strncpy(config.mqttUsername, username.c_str(), sizeof(config.mqttUsername) - 1);
    config.mqttUsername[sizeof(config.mqttUsername) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setMQTTPassword(const String& password) {
    strncpy(config.mqttPassword, password.c_str(), sizeof(config.mqttPassword) - 1);
    config.mqttPassword[sizeof(config.mqttPassword) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setMQTTTopicPrefix(const String& prefix) {
    strncpy(config.mqttTopicPrefix, prefix.c_str(), sizeof(config.mqttTopicPrefix) - 1);
    config.mqttTopicPrefix[sizeof(config.mqttTopicPrefix) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setMQTTRetainMessages(bool retain) {
    config.mqttRetainMessages = retain;
    saveConfig();
}

void ConfigManager::setMQTTKeepAlive(int keepAlive) {
    if (keepAlive > 0 && keepAlive <= 3600) {
        config.mqttKeepAlive = keepAlive;
        saveConfig();
    }
}

// HTTP Server Configuration Methods
void ConfigManager::setServerEnabled(bool enabled) {
    config.serverEnabled = enabled;
    saveConfig();
}

void ConfigManager::setServerUrl(const String& url) {
    strncpy(config.serverUrl, url.c_str(), sizeof(config.serverUrl) - 1);
    config.serverUrl[sizeof(config.serverUrl) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setDeviceId(const String& id) {
    strncpy(config.deviceId, id.c_str(), sizeof(config.deviceId) - 1);
    config.deviceId[sizeof(config.deviceId) - 1] = '\0';
    saveConfig();
}

void ConfigManager::setServerRetryInterval(int seconds) {
    if (seconds >= 60 && seconds <= 86400) { // Min 1 minute, max 24 hours
        config.serverRetryInterval = seconds;
        saveConfig();
    }
}

void ConfigManager::setServerMaxRetries(int retries) {
    if (retries >= 0 && retries <= 100) {
        config.serverMaxRetries = retries;
        saveConfig();
    }
}

void ConfigManager::setMQTTKeepAlive(int keepAlive) {
    if (keepAlive > 0 && keepAlive <= 3600) {
        config.mqttKeepAlive = keepAlive;
        saveConfig();
    }
}