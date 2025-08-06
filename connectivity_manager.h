// ConnectivityManager.h
#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <WiFi.h>
#include <ArduinoMqttClient.h>

class ConnectivityManager {
public:
    struct Config {
        const char* ssid;
        const char* password;
        const char* mqtt_broker;
        int mqtt_port;
        const char* mqtt_user;
        const char* mqtt_pass;
        const char* ntp_server1 = "pool.ntp.org";
        const char* ntp_server2 = "time.nist.gov";
        const char* ntp_server3 = "time.google.com";
        const char* timezone = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    };

    ConnectivityManager(const Config& config);
    ~ConnectivityManager();
    
    // WiFi Management
    bool connectWiFi();
    void disconnectWiFi();
    bool isWiFiConnected() const { return wifi_connected; }
    
    // MQTT Management
    bool connectMQTT();
    void disconnectMQTT();
    bool isMQTTConnected() const { return mqtt_connected; }
    bool publishWithTimeout(const char* topic, const char* message, bool retain = true);
    void poll(); // Call this in main loop when MQTT is connected
    
    // NTP Time Sync
    bool syncTime();
    bool isTimeSynced() const { return time_synced; }
    void getCurrentTimeString(char* buffer, size_t buffer_size);
    
    // Status
    const char* getLastError() const { return last_error; }
    
private:
    Config config_;
    WiFiClient wifi_client_;
    MqttClient mqtt_client_;
    
    bool wifi_connected = false;
    bool mqtt_connected = false;
    bool time_synced = false;
    
    int mqtt_retry_count = 0;
    unsigned long mqtt_connection_start_time = 0;
    unsigned long ntp_sync_start_time = 0;
    
    static const unsigned long NTP_SYNC_TIMEOUT = 10000;
    static const unsigned long MQTT_CONNECTION_TIMEOUT = 10000;
    static const unsigned long MQTT_PUBLISH_TIMEOUT = 5000;
    static const int MAX_MQTT_RETRIES = 3;
    
    char last_error[128];
    
    void setError(const char* error);
};

#endif