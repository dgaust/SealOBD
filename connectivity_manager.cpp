// ConnectivityManager.cpp
#include "ConnectivityManager.h"
#include <Arduino.h>
#include <time.h>

ConnectivityManager::ConnectivityManager(const Config& config) 
    : config_(config), mqtt_client_(wifi_client_) {
    strcpy(last_error, "");
}

ConnectivityManager::~ConnectivityManager() {
    disconnectMQTT();
    disconnectWiFi();
}

void ConnectivityManager::setError(const char* error) {
    strncpy(last_error, error, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    Serial.println(error);
}

bool ConnectivityManager::connectWiFi() {
    if (wifi_connected) {
        return true;
    }
    
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(config_.ssid, config_.password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        Serial.print('.');
        delay(1000);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
        wifi_connected = true;
        return true;
    } else {
        setError("WiFi connection failed");
        return false;
    }
}

void ConnectivityManager::disconnectWiFi() {
    if (wifi_connected) {
        Serial.println("Disconnecting WiFi...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_connected = false;
        Serial.println("WiFi disconnected");
    }
}

bool ConnectivityManager::syncTime() {
    if (time_synced) {
        return true;
    }
    
    if (!wifi_connected) {
        setError("WiFi not connected for NTP sync");
        return false;
    }
    
    Serial.println("Synchronizing time with NTP servers...");
    ntp_sync_start_time = millis();
    
    configTime(0, 0, config_.ntp_server1, config_.ntp_server2, config_.ntp_server3);
    setenv("TZ", config_.timezone, 1);
    tzset();
    
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    
    while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
        if (millis() - ntp_sync_start_time > NTP_SYNC_TIMEOUT) {
            setError("NTP sync timeout");
            return false;
        }
        
        Serial.print("Waiting for system time to be set... (");
        Serial.print(retry);
        Serial.print("/");
        Serial.print(retry_count);
        Serial.println(")");
        delay(1000);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    
    if (timeinfo.tm_year < (2024 - 1900)) {
        setError("Failed to obtain time from NTP servers");
        return false;
    }
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    Serial.print("Time synchronized: ");
    Serial.println(time_str);
    
    time_synced = true;
    return true;
}

void ConnectivityManager::getCurrentTimeString(char* buffer, size_t buffer_size) {
    if (!time_synced) {
        strncpy(buffer, "TIME_NOT_SYNCED", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
}

bool ConnectivityManager::connectMQTT() {
    if (mqtt_connected) {
        return true;
    }
    
    if (!wifi_connected) {
        setError("WiFi not connected for MQTT");
        return false;
    }
    
    if (mqtt_retry_count == 0) {
        mqtt_connection_start_time = millis();
    }
    
    // Check for overall timeout
    if (millis() - mqtt_connection_start_time > MQTT_CONNECTION_TIMEOUT) {
        mqtt_retry_count++;
        
        if (mqtt_retry_count >= MAX_MQTT_RETRIES) {
            setError("MQTT connection: Maximum retries reached");
            mqtt_retry_count = 0;
            return false;
        } else {
            Serial.print("Retrying MQTT connection (attempt ");
            Serial.print(mqtt_retry_count + 1);
            Serial.print("/");
            Serial.print(MAX_MQTT_RETRIES);
            Serial.println(")");
            mqtt_connection_start_time = millis();
        }
    }
    
    Serial.println("Attempting MQTT connection...");
    mqtt_client_.setUsernamePassword(config_.mqtt_user, config_.mqtt_pass);
    mqtt_client_.setConnectionTimeout(5000);
    
    if (!mqtt_client_.connect(config_.mqtt_broker, config_.mqtt_port)) {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "MQTT connection failed! Error code = %d", mqtt_client_.connectError());
        setError(error_msg);
        return false;
    }
    
    Serial.println("Connected to MQTT broker!");
    mqtt_connected = true;
    mqtt_retry_count = 0;
    return true;
}

void ConnectivityManager::disconnectMQTT() {
    if (mqtt_connected) {
        Serial.println("Disconnecting MQTT...");
        mqtt_client_.stop();
        mqtt_connected = false;
        Serial.println("MQTT disconnected");
    }
}

bool ConnectivityManager::publishWithTimeout(const char* topic, const char* message, bool retain) {
    if (!mqtt_connected) {
        setError("MQTT not connected for publish");
        return false;
    }
    
    unsigned long publish_start = millis();
    
    Serial.print("Publishing to ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(message);
    
    if (!mqtt_client_.beginMessage(topic, retain)) {
        setError("Failed to begin MQTT message");
        return false;
    }
    
    if (mqtt_client_.print(message) == 0) {
        setError("Failed to write MQTT message");
        return false;
    }
    
    if (!mqtt_client_.endMessage()) {
        setError("Failed to send MQTT message");
        return false;
    }
    
    // Check if publish took too long
    if (millis() - publish_start > MQTT_PUBLISH_TIMEOUT) {
        setError("MQTT publish timeout");
        return false;
    }
    
    return true;
}

void ConnectivityManager::poll() {
    if (mqtt_connected && WiFi.status() == WL_CONNECTED) {
        unsigned long poll_start = millis();
        mqtt_client_.poll();
        
        // If polling takes too long, assume connection issues
        if (millis() - poll_start > 1000) {
            setError("MQTT poll timeout");
            mqtt_connected = false;
        }
    }
}