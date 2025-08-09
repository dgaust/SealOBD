#ifndef MQTT_NETWORK_MANAGER_H
#define MQTT_NETWORK_MANAGER_H

#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include "Config.h"
#include "Logger.h"

class MQTTNetworkManager {
public:
    MQTTNetworkManager();
    ~MQTTNetworkManager();
    
    // WiFi Management
    bool connectWiFi();
    void disconnectWiFi();
    bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
    
    // MQTT Management
    bool connectMQTT();
    void disconnectMQTT();
    bool isMQTTConnected() { return mqttClient.connected(); }
    void pollMQTT();
    
    // Publishing Methods
    bool publishFloat(const char* topic, float value, bool retain = true);
    bool publishString(const char* topic, const char* message, bool retain = true);
    bool publishStatus(const char* status);
    bool publishLastUpdate(const char* timestamp);
    
private:
    WiFiClient wifiClient;
    MqttClient mqttClient;
};

#endif // MQTT_NETWORK_MANAGER_H