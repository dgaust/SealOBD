#include "MQTTNetworkManager.h"

MQTTNetworkManager::MQTTNetworkManager() : mqttClient(wifiClient) {
}

MQTTNetworkManager::~MQTTNetworkManager() {
    disconnectMQTT();
    disconnectWiFi();
}

bool MQTTNetworkManager::connectWiFi() {
    if (isWiFiConnected()) {
        LOG_DEBUG("WiFi already connected");
        return true;
    }
    
    LOG_INFO("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFi_Config::SSID, WiFi_Config::PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > 30000) {
            LOG_ERROR("WiFi connection timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    LOG_INFO_F("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

void MQTTNetworkManager::disconnectWiFi() {
    if (isWiFiConnected()) {
        LOG_INFO("Disconnecting WiFi...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        LOG_INFO("WiFi disconnected");
    }
}

bool MQTTNetworkManager::connectMQTT() {
    if (isMQTTConnected()) {
        LOG_DEBUG("MQTT already connected");
        return true;
    }
    
    if (!isWiFiConnected()) {
        LOG_ERROR("Cannot connect MQTT - WiFi not connected");
        return false;
    }
    
    LOG_INFO("Connecting to MQTT broker...");
    mqttClient.setUsernamePassword(MQTT::USER, MQTT::PASS);
    
    if (!mqttClient.connect(MQTT::BROKER, MQTT::PORT)) {
        LOG_ERROR_F("MQTT connection failed! Error code = %d", mqttClient.connectError());
        return false;
    }
    
    LOG_INFO("Connected to MQTT broker");
    return true;
}

void MQTTNetworkManager::disconnectMQTT() {
    if (isMQTTConnected()) {
        LOG_INFO("Disconnecting MQTT...");
        mqttClient.stop();
        LOG_INFO("MQTT disconnected");
    }
}

void MQTTNetworkManager::pollMQTT() {
    if (isMQTTConnected()) {
        mqttClient.poll();
    }
}

bool MQTTNetworkManager::publishFloat(const char* topic, float value, bool retain) {
    if (!isMQTTConnected()) {
        LOG_ERROR("Cannot publish - MQTT not connected");
        return false;
    }
    
    mqttClient.beginMessage(topic, retain, MQTT::QOS);
    mqttClient.print(value);
    mqttClient.endMessage();
    
    LOG_INFO_F("Published to %s: %.2f", topic, value);
    return true;
}

bool MQTTNetworkManager::publishString(const char* topic, const char* message, bool retain) {
    if (!isMQTTConnected()) {
        LOG_ERROR("Cannot publish - MQTT not connected");
        return false;
    }
    
    mqttClient.beginMessage(topic, retain, MQTT::QOS);
    mqttClient.print(message);
    mqttClient.endMessage();
    
    LOG_INFO_F("Published to %s: %s", topic, message);
    return true;
}

bool MQTTNetworkManager::publishStatus(const char* status) {
    return publishString(MQTT::TOPIC_STATUS, status, MQTT::RETAIN);
}

bool MQTTNetworkManager::publishLastUpdate(const char* timestamp) {
    return publishString(MQTT::TOPIC_LAST_UPDATE, timestamp, MQTT::RETAIN);
}