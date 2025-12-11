#include "mqtt_manager.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "rtc_module.h"
#include <ArduinoJson.h>

// Static instance for callback
MQTTManager* MQTTManager::instance = nullptr;

MQTTManager::MQTTManager() : mqttClient(wifiClient) {
    instance = this;
    configManager = nullptr;
    scheduleManager = nullptr;
    rtcModule = nullptr;
    lastReconnectAttempt = 0;
    lastStatusPublish = 0;
    lastMinutePublished = -1;  // Initialize to -1 to force first publish
    isConnected = false;
    deviceId = "esp32_irrigation";
}

bool MQTTManager::begin(ConfigManager* config, ScheduleManager* schedule, RTCModule* rtc) {
    configManager = config;
    scheduleManager = schedule;
    rtcModule = rtc;

    if (!configManager || !scheduleManager || !rtcModule) {
        Serial.println("MQTT: Invalid managers provided");
        return false;
    }

    // Set larger buffer size for MQTT messages
    mqttClient.setBufferSize(1024);  // Increase from default 256 to 1024 bytes
    Serial.println("MQTT: Buffer size set to 1024 bytes");

    // Set callback
    mqttClient.setCallback(staticOnMessage);

    // Update configuration
    updateConfig();

    Serial.println("MQTT Manager initialized");
    return true;
}

void MQTTManager::updateConfig() {
    if (!configManager || !configManager->isMQTTEnabled()) {
        return;
    }

    // Get actual device ID from config
    deviceId = configManager->getDeviceId();
    if (deviceId.isEmpty()) {
        deviceId = "esp32_irrigation";
    }

    topicPrefix = configManager->getMQTTTopicPrefix();
    if (!topicPrefix.endsWith("/")) {
        topicPrefix += "/";
    }

    String broker = configManager->getMQTTBroker();
    uint16_t port = configManager->getMQTTPort();

    Serial.println("MQTT: Configuration updated");
    Serial.println("  Broker: " + broker);
    Serial.println("  Port: " + String(port));
    Serial.println("  Device ID: " + deviceId);
    Serial.println("  Topic Prefix: " + topicPrefix);

    mqttClient.setServer(broker.c_str(), port);
    mqttClient.setKeepAlive(configManager->getMQTTKeepAlive());
}

void MQTTManager::loop() {
    if (!configManager || !configManager->isMQTTEnabled()) {
        return;
    }

    if (!mqttClient.connected()) {
        isConnected = false;
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        isConnected = true;
        mqttClient.loop();

        // Publish status every minute on the minute using RTC clock
        if (rtcModule) {
            DateTime currentTime = rtcModule->getCurrentTime();
            int currentMinute = currentTime.minute();

            // Check if we've moved to a new minute
            if (currentMinute != lastMinutePublished) {
                lastMinutePublished = currentMinute;
                lastStatusPublish = millis();  // Update for compatibility

                char timeStr[10];
                sprintf(timeStr, "%02d:%02d", currentTime.hour(), currentTime.minute());
                Serial.println("MQTT: Publishing periodic status at " + String(timeStr));

                publishStatus();
                publishDeviceConfig();  // Publish device configuration including IP
            }
        } else {
            // Fallback to 30-second interval if RTC not available
            unsigned long now = millis();
            if (now - lastStatusPublish > 30000) {
                lastStatusPublish = now;
                publishStatus();
                publishDeviceConfig();  // Publish device configuration including IP
            }
        }
    }
}

bool MQTTManager::reconnect() {
    if (!configManager || !configManager->isMQTTEnabled()) {
        return false;
    }

    // Check WiFi connectivity first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("MQTT: WiFi not connected, cannot connect to MQTT");
        return false;
    }

    String clientId = getClientId();
    String username = configManager->getMQTTUsername();
    String password = configManager->getMQTTPassword();
    String broker = configManager->getMQTTBroker();
    uint16_t port = configManager->getMQTTPort();

    Serial.println("MQTT: Attempting connection as " + clientId);
    Serial.println("MQTT: Connecting to " + broker + ":" + String(port));
    Serial.println("MQTT: WiFi status: " + String(WiFi.status()));
    Serial.println("MQTT: Local IP: " + WiFi.localIP().toString());

    // Set server configuration directly without calling updateConfig
    mqttClient.setServer(broker.c_str(), port);

    bool connected = false;
    if (username.length() > 0) {
        Serial.println("MQTT: Connecting with credentials...");
        connected = mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str());
    } else {
        Serial.println("MQTT: Connecting without credentials...");
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        isConnected = true;
        Serial.println("MQTT: Connected");
        lastMinutePublished = -1;  // Reset to force immediate status publish
        subscribeToTopics();
        Serial.println("MQTT: Publishing initial status...");
        publishStatus();
        Serial.println("MQTT: Publishing Configuration...");
        publishDeviceConfig();  // Publish device configuration including IP
        return true;
    } else {
        Serial.println("MQTT: Connection failed, rc=" + String(mqttClient.state()));
        return false;
    }
}

void MQTTManager::subscribeToTopics() {
    // Subscribe to configuration topics
    String configTopic = topicPrefix + deviceId + "/config/+/set";
    mqttClient.subscribe(configTopic.c_str());
    Serial.println("MQTT: Subscribed to " + configTopic);

    // Subscribe to command topics
    String commandTopic = topicPrefix + deviceId + "/command/+";
    mqttClient.subscribe(commandTopic.c_str());
    Serial.println("MQTT: Subscribed to " + commandTopic);

    // Subscribe to schedule topics
    String scheduleTopic = topicPrefix + deviceId + "/schedule/set";
    mqttClient.subscribe(scheduleTopic.c_str());
    Serial.println("MQTT: Subscribed to " + scheduleTopic);

    String aiScheduleTopic = topicPrefix + deviceId + "/schedule/ai/set";
    mqttClient.subscribe(aiScheduleTopic.c_str());
    Serial.println("MQTT: Subscribed to " + aiScheduleTopic);

    // Subscribe to zone control topics
    String zoneTopic = topicPrefix + deviceId + "/zone/+/set";
    mqttClient.subscribe(zoneTopic.c_str());
    Serial.println("MQTT: Subscribed to " + zoneTopic);
}

void MQTTManager::staticOnMessage(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->onMessage(topic, payload, length);
    }
}

void MQTTManager::onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";

    for (int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    Serial.println("MQTT: Received [" + topicStr + "] " + payloadStr);

    // Route message to appropriate handler
    if (topicStr.indexOf("/config/") >= 0) {
        handleConfigMessage(topicStr, payloadStr);
    } else if (topicStr.indexOf("/command/") >= 0) {
        handleCommandMessage(topicStr, payloadStr);
    } else if (topicStr.indexOf("/schedule/") >= 0) {
        handleScheduleMessage(topicStr, payloadStr);
    } else if (topicStr.indexOf("/zone/") >= 0) {
        handleZoneMessage(topicStr, payloadStr);
    }
}

void MQTTManager::handleConfigMessage(const String& topic, const String& payload) {
    // Extract setting name from topic
    int configIndex = topic.indexOf("/config/");
    int setIndex = topic.indexOf("/set");
    if (configIndex < 0 || setIndex < 0) return;

    String setting = topic.substring(configIndex + 8, setIndex);

    if (setting == "timezone") {
        float tz = payload.toFloat();
        int halfHours = (int)(tz * 2);
        configManager->setTimezoneOffset(halfHours);
        publishConfig();
    } else if (setting == "mqtt_broker") {
        configManager->setMQTTBroker(payload);
        updateConfig();
        publishConfig();
    } else if (setting == "mqtt_port") {
        configManager->setMQTTPort(payload.toInt());
        updateConfig();
        publishConfig();
    } else if (setting == "mqtt_username") {
        configManager->setMQTTUsername(payload);
        updateConfig();
        publishConfig();
    } else if (setting == "mqtt_topic_prefix") {
        configManager->setMQTTTopicPrefix(payload);
        updateConfig();
        publishConfig();
    } else if (setting == "max_enabled_zones") {
        configManager->setMaxEnabledZones(payload.toInt());
        publishConfig();
    }

    Serial.println("MQTT: Updated config setting: " + setting + " = " + payload);
}

void MQTTManager::handleCommandMessage(const String& topic, const String& payload) {
    // Extract command from topic
    int commandIndex = topic.indexOf("/command/");
    if (commandIndex < 0) return;

    String command = topic.substring(commandIndex + 9);

    if (command == "restart") {
        Serial.println("MQTT: Restart command received");
        ESP.restart();
    } else if (command == "status") {
        publishDeviceStatus();
    } else if (command == "rain_delay") {
        int minutes = payload.toInt();
        if (minutes > 0) {
            scheduleManager->setRainDelay(minutes);
            publishStatus();
        }
    } else if (command == "clear_rain") {
        scheduleManager->clearRainDelay();
        publishStatus();
    } else if (command == "enable_schedule") {
        bool enabled = (payload == "true" || payload == "1");
        configManager->setSchedulingEnabled(enabled);
        publishStatus();
    }

    Serial.println("MQTT: Executed command: " + command + " with payload: " + payload);
}

void MQTTManager::handleZoneMessage(const String& topic, const String& payload) {
    // Extract zone number from topic (format: .../zone/X/set)
    int zoneIndex = topic.indexOf("/zone/");
    int setIndex = topic.indexOf("/set");
    if (zoneIndex < 0 || setIndex < 0) return;

    String zoneStr = topic.substring(zoneIndex + 6, setIndex);
    uint8_t zone = zoneStr.toInt();

    if (zone < 1 || zone > configManager->getMaxEnabledZones()) {
        Serial.println("MQTT: Invalid zone number: " + String(zone));
        return;
    }

    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.println("MQTT: Failed to parse zone command JSON");
        return;
    }

    String action = doc["action"] | "";
    int duration = doc["duration"] | 0;

    if (action == "ON" && duration > 0) {
        // Enforce maximum duration of 75 minutes for manual MQTT commands
        if (duration > 75) {
            Serial.println("MQTT: Duration " + String(duration) + " exceeds maximum of 75 minutes, capping to 75");
            duration = 75;
        }
        Serial.println("MQTT: Starting zone " + String(zone) + " for " + String(duration) + " minutes (manual)");
        scheduleManager->startZoneManual(zone, duration);
    } else if (action == "OFF") {
        Serial.println("MQTT: Stopping zone " + String(zone));
        scheduleManager->stopZone(zone);
    } else {
        Serial.println("MQTT: Invalid zone command - action: " + action + ", duration: " + String(duration));
    }
}

void MQTTManager::handleScheduleMessage(const String& topic, const String& payload) {
    if (topic.indexOf("/schedule/ai/set") >= 0) {
        // Handle AI schedule update
        bool success = scheduleManager->updateScheduleFromJSON(payload);
        String responseTopic = topicPrefix + deviceId + "/schedule/ai/result";
        String response = success ? "success" : "error";
        mqttClient.publish(responseTopic.c_str(), response.c_str());
    } else if (topic.indexOf("/schedule/set") >= 0) {
        // Handle general schedule command
        bool success = scheduleManager->updateScheduleFromJSON(payload);
        String responseTopic = topicPrefix + deviceId + "/schedule/result";
        String response = success ? "success" : "error";
        mqttClient.publish(responseTopic.c_str(), response.c_str());
    }

    publishScheduleStatus();
}

String MQTTManager::buildTopic(const String& suffix) {
    return topicPrefix + deviceId + "/" + suffix;
}

String MQTTManager::buildConfigTopic(const String& setting) {
    return buildTopic("config/" + setting);
}

String MQTTManager::buildStatusTopic(const String& type) {
    return buildTopic(type);
}

void MQTTManager::publishStatus() {
    if (!isConnected) return;

    publishDeviceStatus();
    publishScheduleStatus();
}

void MQTTManager::publishDeviceStatus() {
    if (!isConnected) return;

    String statusJson = scheduleManager->getDeviceStatusJSON();
    String topic = buildStatusTopic("device");

    bool retain = configManager->isMQTTRetainMessages();
    mqttClient.publish(topic.c_str(), statusJson.c_str(), retain);
    Serial.println("MQTT: Published device status to " + topic);
}

void MQTTManager::publishScheduleStatus() {
    if (!isConnected) return;

    String schedulesJson = scheduleManager->getSchedulesJSON();
    String topic = buildStatusTopic("schedules");

    bool retain = configManager->isMQTTRetainMessages();
    mqttClient.publish(topic.c_str(), schedulesJson.c_str(), retain);
}

void MQTTManager::publishConfig() {
    if (!isConnected) return;

    String configJson = configManager->getConfigJSON();
    String topic = buildStatusTopic("config");

    bool retain = configManager->isMQTTRetainMessages();
    mqttClient.publish(topic.c_str(), configJson.c_str(), retain);
}

void MQTTManager::publishDeviceConfig() {
    if (!isConnected) {
        Serial.println("MQTT: Skipping publishDeviceConfig - not connected");
        return;
    }

    JsonDocument doc;
    DateTime utcTime = rtcModule->getCurrentTime();
    int offsetSeconds = configManager->getTimezoneOffset() * 1800; // Convert half-hours to seconds
    // Apply daylight saving time (add 1 hour if enabled)
    if (configManager->isDaylightSaving()) {
        offsetSeconds += 3600;
    }
    DateTime localTime = DateTime(utcTime.unixtime() + offsetSeconds);
    char timeBuffer[32];
    int offsetHours = offsetSeconds / 3600;
    int offsetMins = abs(offsetSeconds) % 3600 / 60;
    sprintf(timeBuffer, "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
            localTime.year(), localTime.month(), localTime.day(),
            localTime.hour(), localTime.minute(), localTime.second(),
            offsetHours, offsetMins);

    doc["device_id"] = deviceId;
    doc["client_id"] = getClientId();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["timestamp"] = timeBuffer;
    doc["uptime"] = millis();
    doc["firmware_version"] = "1.0.0";
    doc["mqtt_broker"] = configManager->getMQTTBroker();
    doc["mqtt_port"] = configManager->getMQTTPort();
    doc["topic_prefix"] = configManager->getMQTTTopicPrefix();
    doc["timezone"] = configManager->getTimezoneOffset() / 2.0;
    doc["daylight_saving"] = configManager->isDaylightSaving();
    doc["max_zones"] = configManager->getMaxEnabledZones();

    String json;
    serializeJson(doc, json);

    String topic = buildTopic("config/device");
    bool retain = configManager->isMQTTRetainMessages();

    bool published = mqttClient.publish(topic.c_str(), json.c_str(), retain);

    if (!published) {
        Serial.println("MQTT: Failed to publish device configuration to " + topic);
        Serial.println("MQTT: MQTT client state: " + String(mqttClient.state()));
    }
}

bool MQTTManager::isClientConnected() {
    return isConnected && mqttClient.connected();
}

String MQTTManager::getClientId() {
    String clientId = deviceId + "_" + String(WiFi.macAddress());
    clientId.replace(":", "");
    return clientId;
}

void MQTTManager::setDeviceId(const String& id) {
    deviceId = id;
}

void MQTTManager::publishZoneStatus(uint8_t zone, const String& status, uint32_t duration, uint8_t scheduleId, const String& eventType) {
    if (!isConnected) return;

    JsonDocument doc;
    DateTime utcTime = rtcModule->getCurrentTime();
    char timeBuffer[32];
    sprintf(timeBuffer, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            utcTime.year(), utcTime.month(), utcTime.day(),
            utcTime.hour(), utcTime.minute(), utcTime.second());

    doc["device_id"] = deviceId;
    doc["device_zone_number"] = zone;
    doc["event"] = status;
    doc["timestamp_utc"] = timeBuffer;
    doc["event_type"] = eventType;

    if (status == "start") {
        doc["duration_planned_min"] = duration;
        if (scheduleId > 0) {
            doc["schedule_id"] = scheduleId;
        }
    } else if (status == "stop") {
        doc["completed"] = true; // Assume normal completion
    }

    String json;
    serializeJson(doc, json);

    String topic = buildStatusTopic("zone/" + String(zone));
    bool retain = false; // Don't retain event messages

    bool published = mqttClient.publish(topic.c_str(), json.c_str(), retain);

    if (published) {
        Serial.println("MQTT: Published zone " + String(zone) + " " + status + " event");
    } else {
        Serial.println("MQTT: Failed to publish zone event to " + topic);
    }
}