#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Forward declarations
class ConfigManager;
class ScheduleManager;
class RTCModule;

class MQTTManager {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    ConfigManager* configManager;
    ScheduleManager* scheduleManager;
    RTCModule* rtcModule;

    unsigned long lastReconnectAttempt;
    unsigned long lastStatusPublish;
    int lastMinutePublished;  // Track last minute when status was published
    bool isConnected;

    // Topic constants
    String topicPrefix;
    String deviceId;

    // Callback handling
    void onMessage(char* topic, byte* payload, unsigned int length);
    static void staticOnMessage(char* topic, byte* payload, unsigned int length);
    static MQTTManager* instance;

    // Connection management
    bool reconnect();
    void updateTopics();

    // Message handling
    void handleConfigMessage(const String& topic, const String& payload);
    void handleCommandMessage(const String& topic, const String& payload);
    void handleScheduleMessage(const String& topic, const String& payload);

    // Topic builders
    String buildTopic(const String& suffix);
    String buildConfigTopic(const String& setting);
    String buildStatusTopic(const String& type);

public:
    MQTTManager();

    // Initialization
    bool begin(ConfigManager* config, ScheduleManager* schedule, RTCModule* rtc);

    // Connection management
    void loop();
    bool connect();
    void disconnect();
    bool isClientConnected();

    // Publishing
    void publishStatus();
    void publishDeviceStatus();
    void publishZoneStatus(uint8_t zone, const String& status, uint32_t timeRemaining = 0);
    void publishScheduleStatus();
    void publishConfig();
    void publishDeviceConfig();  // Publish device configuration including IP

    // Configuration
    void updateConfig();
    void subscribeToTopics();

    // Utility
    String getClientId();
    void setDeviceId(const String& id);
};

#endif // MQTT_MANAGER_H