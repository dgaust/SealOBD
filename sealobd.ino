#include <M5Unified.h>
#include "Config.h"
#include "Logger.h"
#include "OBDManager.h"
#include "MQTTNetworkManager.h"
#include "TimeManager.h"

// Global Objects
OBDManager obdManager;
MQTTNetworkManager networkManager;
TimeManager timeManager;

// State Management
AppState currentState = AppState::OBD_SETUP;
unsigned long lastUpdateTime = 0;
unsigned long updateInterval = Intervals::INITIAL_DELAY;

// Vehicle Data
VehicleData vehicleData;

// Function declarations
void processStateMachine();
void handleOBDSetup();
void handleOBDReadSOC();
void handleOBDReadTemp();
void handleOBDReadVoltage();
void handleOBDReadTotalCharges();
void handleOBDReadKwhCharged();
void handleOBDReadKwhDischarged();
void handleWiFiConnect();
void handleNTPSync();
void handleMQTTConnect();
void handleMQTTPublish();
void handleWaitCycle();
void handleError(const char* errorMessage);
void cleanup();

void setup() {
    // Initialize logging
    Logger::begin(DEBUG_BAUD_RATE);
    Logger::setLevel(LogLevel::INFO);
    
    LOG_INFO("=================================");
    LOG_INFO("SealOBD System Starting...");
    LOG_INFO("=================================");
    LOG_INFO_F("Version: 2.0.0");
    LOG_INFO_F("Normal Update Interval: %d ms", Intervals::NORMAL_UPDATE);
    LOG_INFO_F("Error Retry Interval: %d ms", Intervals::ERROR_RETRY);
}

void loop() {
    // Poll MQTT if connected
    networkManager.pollMQTT();
    
    // Check if it's time for an update
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= updateInterval) {
        processStateMachine();
        lastUpdateTime = currentTime;
    }
}

void processStateMachine() {
    switch (currentState) {
        case AppState::OBD_SETUP:
            handleOBDSetup();
            break;
            
        case AppState::OBD_READ_SOC:
            handleOBDReadSOC();
            break;
            
        case AppState::OBD_READ_BATTERY_TEMP:
            handleOBDReadTemp();
            break;
            
        case AppState::OBD_READ_BATTERY_VOLTAGE:
            handleOBDReadVoltage();
            break;

        case AppState::OBD_CHARGE_TIMES:
            handleOBDReadTotalCharges();
            break;
            
        case AppState::OBD_TOTAL_CHARGED_KWH:
            handleOBDReadKwhCharged();
            break;
            
        case AppState::OBT_TOTAL_DISCHARGED_KWH:
            handleOBDReadKwhDischarged();
            break;
            
        case AppState::WIFI_CONNECT:
            handleWiFiConnect();
            break;
            
        case AppState::NTP_SYNC:
            handleNTPSync();
            break;
            
        case AppState::MQTT_CONNECT:
            handleMQTTConnect();
            break;
            
        case AppState::MQTT_PUBLISH:
            handleMQTTPublish();
            break;
            
        case AppState::WAIT_CYCLE:
            handleWaitCycle();
            break;
    }
}

void handleOBDSetup() {
    LOG_INFO("Step 1: Setting up OBD connection...");
    
    if (obdManager.connect()) {
        LOG_INFO("OBD connection successful");
        currentState = AppState::OBD_READ_SOC;
    } else {
        LOG_ERROR("OBD connection failed");
        handleError(obdManager.isCarConnectionLost() ? 
                   ErrorMessages::NO_CAR : 
                   ErrorMessages::BLE_TIMEOUT);
    }
}

void handleOBDReadSOC() {
    LOG_INFO("Step 2: Reading State of Charge...");
    
    if (obdManager.readStateOfCharge(vehicleData.stateOfCharge)) {
        currentState = AppState::OBD_READ_BATTERY_TEMP;
    } else {
        handleError(ErrorMessages::SOC_FAILED);
    }
}

void handleOBDReadTemp() {
    LOG_INFO("Step 3: Reading Battery Temperature...");
    
    if (obdManager.readBatteryTemperature(vehicleData.batteryTemperature)) {
        currentState = AppState::OBD_READ_BATTERY_VOLTAGE;
    } else {
        handleError(ErrorMessages::TEMP_FAILED);
    }
}

void handleOBDReadVoltage() {
    LOG_INFO("Step 4: Reading Battery Voltage...");
    
    if (obdManager.readBatteryVoltage(vehicleData.batteryVoltage)) {
        currentState = AppState::OBD_CHARGE_TIMES;
    } else {
        handleError(ErrorMessages::VOLTAGE_FAILED);
    }
}

void handleOBDReadTotalCharges() {
    LOG_INFO("Step 5: Reading total amount of charges...");
    
    if (obdManager.readTotalCharges(vehicleData.totalCharges)) {
        currentState = AppState::OBD_TOTAL_CHARGED_KWH;
    } else {
        handleError(ErrorMessages::TIMES_CHARGED_FAILED);
    }
}

void handleOBDReadKwhCharged() {
    LOG_INFO("Step 6: Reading total kWh charged...");
    
    if (obdManager.readTotalKwhCharged(vehicleData.totalKwhCharged)) {
        currentState = AppState::OBT_TOTAL_DISCHARGED_KWH;
    } else {
        handleError(ErrorMessages::TOTAL_KWH_CHARGED_FAILED);
    }
}

void handleOBDReadKwhDischarged() {
    LOG_INFO("Step 7: Reading total kWh discharged...");
    
    if (obdManager.readTotalKwhDischarged(vehicleData.totalKwhDischarged)) {
        vehicleData.isValid = true;
        currentState = AppState::WIFI_CONNECT;
    } else {
        handleError(ErrorMessages::TOTAL_KWH_DISCHARGED_FAILED);
    }
}

void handleWiFiConnect() {
    LOG_INFO("Step 8: Connecting to WiFi...");
    
    if (networkManager.connectWiFi()) {
        currentState = AppState::NTP_SYNC;
    } else {
        LOG_ERROR("WiFi connection failed, retrying next cycle");
        currentState = AppState::WAIT_CYCLE;
        updateInterval = Intervals::ERROR_RETRY;
    }
}

void handleNTPSync() {
    LOG_INFO("Step 9: Synchronizing time...");
    
    if (timeManager.syncWithNTP()) {
        currentState = AppState::MQTT_CONNECT;
    } else {
        LOG_WARNING("NTP sync failed, continuing without time sync");
        currentState = AppState::MQTT_CONNECT;
    }
}

void handleMQTTConnect() {
    LOG_INFO("Step 10: Connecting to MQTT broker...");
    
    if (networkManager.connectMQTT()) {
        currentState = AppState::MQTT_PUBLISH;
    } else {
        LOG_ERROR("MQTT connection failed");
        cleanup();
        currentState = AppState::WAIT_CYCLE;
        updateInterval = Intervals::ERROR_RETRY;
    }
}

void handleMQTTPublish() {
    LOG_INFO("Step 11: Publishing data to MQTT...");
    
    // Publish status
    const char* status = obdManager.isCarConnectionLost() ? 
                        ErrorMessages::NO_CAR : 
                        ErrorMessages::CONNECTED;
    networkManager.publishStatus(status);
    
    // Publish vehicle data if valid
    if (vehicleData.isValid && !obdManager.isCarConnectionLost()) {
        networkManager.publishFloat(MQTT::TOPIC_SOC, vehicleData.stateOfCharge);
        networkManager.publishFloat(MQTT::TOPIC_TEMP, vehicleData.batteryTemperature);
        networkManager.publishFloat(MQTT::TOPIC_VOLTAGE, vehicleData.batteryVoltage);
        networkManager.publishFloat(MQTT::TOPIC_CHARGES_UPDATE, vehicleData.totalCharges);
        networkManager.publishFloat(MQTT::TOPIC_KWH_CHARGED_UPDATE, vehicleData.totalKwhCharged);
        networkManager.publishFloat(MQTT::TOPIC_KWH_DISCHARGED_UPDATE, vehicleData.totalKwhDischarged);
        
        LOG_INFO("=== Vehicle Data Published ===");
        LOG_INFO_F("SoC: %.2f%%", vehicleData.stateOfCharge);
        LOG_INFO_F("Temperature: %.1f°C", vehicleData.batteryTemperature);
        LOG_INFO_F("Voltage: %.2fV", vehicleData.batteryVoltage);
        LOG_INFO_F("Total charges: %.0f", vehicleData.totalCharges);
        LOG_INFO_F("Total kWh charged: %.2f kWh", vehicleData.totalKwhCharged);
        LOG_INFO_F("Total kWh discharged: %.2f kWh", vehicleData.totalKwhDischarged);
        LOG_INFO("==============================");
    }
    
    // Publish timestamp
    String timestamp = timeManager.getCurrentTimestamp();
    networkManager.publishLastUpdate(timestamp.c_str());
    
    // Cleanup and prepare for next cycle
    cleanup();
    currentState = AppState::WAIT_CYCLE;
    updateInterval = Intervals::NORMAL_UPDATE;
    
    LOG_INFO("Cycle complete, waiting for next update...");
}

void handleWaitCycle() {
    LOG_INFO("Starting new cycle...");
    currentState = AppState::OBD_SETUP;
    updateInterval = Intervals::INITIAL_DELAY;
}

void handleError(const char* errorMessage) {
    LOG_ERROR_F("Error occurred: %s", errorMessage);
    
    // Try to report error via MQTT if possible
    if (!networkManager.isWiFiConnected()) {
        networkManager.connectWiFi();
    }
    
    if (!timeManager.isSynced() && networkManager.isWiFiConnected()) {
        timeManager.syncWithNTP();
    }
    
    if (!networkManager.isMQTTConnected() && networkManager.isWiFiConnected()) {
        networkManager.connectMQTT();
    }
    
    if (networkManager.isMQTTConnected()) {
        networkManager.publishStatus(errorMessage);
        String timestamp = timeManager.getCurrentTimestamp();
        networkManager.publishLastUpdate(timestamp.c_str());
    }
    
    // Cleanup and retry
    cleanup();
    currentState = AppState::WAIT_CYCLE;
    updateInterval = Intervals::ERROR_RETRY;
}

void cleanup() {
    LOG_DEBUG("Performing cleanup...");
    
    // Disconnect in reverse order
    networkManager.disconnectMQTT();
    networkManager.disconnectWiFi();
    obdManager.disconnect();
    
    // Reset vehicle data
    vehicleData.isValid = false;
    
    LOG_DEBUG("Cleanup complete");
}