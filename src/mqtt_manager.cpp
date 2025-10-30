#include "mqtt_manager.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include <ArduinoJson.h>

// Static instance for callback
MQTTManager* MQTTManager::instance = nullptr;

MQTTManager::MQTTManager() : mqttClient(wifiClient) {
    instance = this;
    configManager = nullptr;
    scheduleManager = nullptr;
    lastReconnectAttempt = 0;
    lastStatusPublish = 0;
    isConnected = false;
    deviceId = "esp32_irrigation";
}

bool MQTTManager::begin(ConfigManager* config, ScheduleManager* schedule) {
    configManager = config;
    scheduleManager = schedule;

    if (!configManager || !scheduleManager) {
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

    topicPrefix = configManager->getMQTTTopicPrefix();
    if (!topicPrefix.endsWith("/")) {
        topicPrefix += "/";
    }

    String broker = configManager->getMQTTBroker();
    uint16_t port = configManager->getMQTTPort();

    Serial.println("MQTT: Configuration updated");
    Serial.println("  Broker: " + broker);
    Serial.println("  Port: " + String(port));
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

        // Publish status periodically (every 30 seconds)
        unsigned long now = millis();
        if (now - lastStatusPublish > 30000) {
            lastStatusPublish = now;
            publishStatus();
            publishDeviceConfig();  // Publish device configuration including IP
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
        subscribeToTopics();
        Serial.println("MQTT: Publishing initial status...");
        publishDiscovery();
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
    mqttClient.subscribe((topicPrefix + deviceId + "/config/+/set").c_str());

    // Subscribe to command topics
    mqttClient.subscribe((topicPrefix + deviceId + "/command/+").c_str());

    // Subscribe to schedule topics
    mqttClient.subscribe((topicPrefix + deviceId + "/schedule/set").c_str());
    mqttClient.subscribe((topicPrefix + deviceId + "/schedule/ai/set").c_str());

    Serial.println("MQTT: Subscribed to command topics");
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
    return buildTopic("status/" + type);
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

void MQTTManager::publishZoneStatus(uint8_t zone, const String& status, uint32_t timeRemaining) {
    if (!isConnected) return;

    JsonDocument doc;
    doc["zone"] = zone;
    doc["status"] = status;
    if (timeRemaining > 0) {
        doc["time_remaining"] = timeRemaining;
    }
    doc["timestamp"] = millis();

    String json;
    serializeJson(doc, json);

    String topic = buildStatusTopic("zone/" + String(zone));
    bool retain = configManager->isMQTTRetainMessages();
    mqttClient.publish(topic.c_str(), json.c_str(), retain);
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
    doc["device_id"] = deviceId;
    doc["client_id"] = getClientId();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    doc["firmware_version"] = "1.0.0";
    doc["mqtt_broker"] = configManager->getMQTTBroker();
    doc["mqtt_port"] = configManager->getMQTTPort();
    doc["topic_prefix"] = configManager->getMQTTTopicPrefix();
    doc["timezone"] = configManager->getTimezoneOffset() / 2.0;
    doc["max_zones"] = configManager->getMaxEnabledZones();

    String json;
    serializeJson(doc, json);

    String topic = buildTopic("config/device");
    bool retain = configManager->isMQTTRetainMessages();

    // Serial.println("MQTT: Publishing to topic: " + topic);
    // Serial.println("MQTT: JSON payload length: " + String(json.length()));
    // Serial.println("MQTT: Payload: " + json);
    // Serial.println("MQTT: Retain: " + String(retain));

    bool published = mqttClient.publish(topic.c_str(), json.c_str(), retain);
    // Serial.println("MQTT: Publish result: " + String(published));

    if (!published) {
    //     Serial.println("MQTT: Successfully published device configuration to " + topic);
    // } else {
        Serial.println("MQTT: Failed to publish device configuration to " + topic);
        Serial.println("MQTT: MQTT client state: " + String(mqttClient.state()));
    }
}

void MQTTManager::publishDiscovery() {
    if (!isConnected) return;

    // Home Assistant auto-discovery
    JsonDocument doc;
    doc["name"] = "ESP32 Irrigation Controller";
    doc["unique_id"] = deviceId;
    doc["device_class"] = "irrigation";
    doc["state_topic"] = buildStatusTopic("device");
    doc["command_topic"] = buildTopic("command/status");

    String json;
    serializeJson(doc, json);

    String discoveryTopic = "homeassistant/switch/" + deviceId + "/config";
    mqttClient.publish(discoveryTopic.c_str(), json.c_str(), true);
    Serial.println("MQTT: Published discovery information");
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