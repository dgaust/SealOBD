#include <M5AtomS3.h>  // Use M5AtomS3 instead of M5Unified
#include "Config.h"
#include "Logger.h"
#include "OBDManager.h"
#include "MQTTNetworkManager.h"
#include "TimeManager.h"
#include "LEDManager.h"

// Global Objects
OBDManager obdManager;
MQTTNetworkManager networkManager;
TimeManager timeManager;
LEDManager ledManager;  // LED manager

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
    // Initialize M5AtomS3 FIRST (this must come before other initializations)
    AtomS3.begin(true);  // Initialize M5AtomS3 Lite with LED enabled
    
    // Small delay to ensure M5AtomS3 is properly initialized
    delay(500);
    
    // Initialize LED manager EARLY so we can show status during startup
    ledManager.begin();
    ledManager.indicateSetup();  // Purple LED during startup
    
    // Initialize logging (now non-blocking)
    Logger::begin(DEBUG_BAUD_RATE);
    Logger::setLevel(LogLevel::INFO);
    
    // Log startup info (will only appear if serial monitor is connected)
    LOG_INFO("=================================");
    LOG_INFO("SealOBD System Starting...");
    LOG_INFO("=================================");
    LOG_INFO_F("Version: 2.0.0");
    LOG_INFO_F("Normal Update Interval: %d ms", Intervals::NORMAL_UPDATE);
    LOG_INFO_F("Error Retry Interval: %d ms", Intervals::ERROR_RETRY);
    
    // Brief startup delay to show startup LED and ensure all systems ready
    delay(2000);
    
    // Show we're ready with a green blink
    ledManager.blink(LED::GREEN, 2, 200);
    
    LOG_INFO("Setup complete, starting main loop...");
}

void loop() {
    // Update M5AtomS3 (required for LED and other functions)
    AtomS3.update();
    
    // Update LED animations
    ledManager.update();
    
    // Poll MQTT if connected (non-blocking)
    networkManager.pollMQTT();
    
    // Check if it's time for an update
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= updateInterval) {
        // Special handling for wait cycle - restart the cycle
        if (currentState == AppState::WAIT_CYCLE) {
            LOG_INFO("Wait cycle complete, starting new cycle...");
            currentState = AppState::OBD_SETUP;
            updateInterval = Intervals::INITIAL_DELAY;
        }
        
        processStateMachine();
        lastUpdateTime = currentTime;
    }
    
    // Small delay to prevent tight loop and reduce power consumption
    delay(50);
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
            
        case AppState::OBD_TOTAL_DISCHARGED_KWH:
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
    ledManager.indicateSetup();  // Purple LED for setup
    
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
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readStateOfCharge(vehicleData.stateOfCharge)) {
        currentState = AppState::OBD_READ_BATTERY_TEMP;
    } else {
        handleError(ErrorMessages::SOC_FAILED);
    }
}

void handleOBDReadTemp() {
    LOG_INFO("Step 3: Reading Battery Temperature...");
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readBatteryTemperature(vehicleData.batteryTemperature)) {
        currentState = AppState::OBD_READ_BATTERY_VOLTAGE;
    } else {
        handleError(ErrorMessages::TEMP_FAILED);
    }
}

void handleOBDReadVoltage() {
    LOG_INFO("Step 4: Reading Battery Voltage...");
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readBatteryVoltage(vehicleData.batteryVoltage)) {
        currentState = AppState::OBD_CHARGE_TIMES;
    } else {
        handleError(ErrorMessages::VOLTAGE_FAILED);
    }
}

void handleOBDReadTotalCharges() {
    LOG_INFO("Step 5: Reading total amount of charges...");
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readTotalCharges(vehicleData.totalCharges)) {
        currentState = AppState::OBD_TOTAL_CHARGED_KWH;
    } else {
        handleError(ErrorMessages::TIMES_CHARGED_FAILED);
    }
}

void handleOBDReadKwhCharged() {
    LOG_INFO("Step 6: Reading total kWh charged...");
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readTotalKwhCharged(vehicleData.totalKwhCharged)) {
        currentState = AppState::OBD_TOTAL_DISCHARGED_KWH;
    } else {
        handleError(ErrorMessages::TOTAL_KWH_CHARGED_FAILED);
    }
}

void handleOBDReadKwhDischarged() {
    LOG_INFO("Step 7: Reading total kWh discharged...");
    ledManager.indicateOBDReading();  // GREEN LED for OBD reading
    
    if (obdManager.readTotalKwhDischarged(vehicleData.totalKwhDischarged)) {
        vehicleData.isValid = true;
        currentState = AppState::WIFI_CONNECT;
    } else {
        handleError(ErrorMessages::TOTAL_KWH_DISCHARGED_FAILED);
    }
}

void handleWiFiConnect() {
    LOG_INFO("Step 8: Connecting to WiFi...");
    ledManager.indicateNetworkOperation();  // Blue LED for network operations
    
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
    ledManager.indicateNetworkOperation();  // Blue LED for network operations
    
    if (timeManager.syncWithNTP()) {
        currentState = AppState::MQTT_CONNECT;
    } else {
        LOG_WARNING("NTP sync failed, continuing without time sync");
        currentState = AppState::MQTT_CONNECT;
    }
}

void handleMQTTConnect() {
    LOG_INFO("Step 10: Connecting to MQTT broker...");
    ledManager.indicateNetworkOperation();  // Blue LED for network operations
    
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
    ledManager.indicateNetworkOperation();  // Blue LED for network operations
    
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
        
        // Brief success indication with green blink
        ledManager.blink(LED::GREEN, 2, 300);
    }
    
    // Publish timestamp
    String timestamp = timeManager.getCurrentTimestamp();
    networkManager.publishLastUpdate(timestamp.c_str());
    
    // Cleanup and prepare for next cycle
    cleanup();
    
    LOG_INFO("Cycle complete, setting up wait cycle...");
    
    // Set up wait cycle
    currentState = AppState::WAIT_CYCLE;
    updateInterval = Intervals::NORMAL_UPDATE;
    
    // Set LED to yellow for waiting
    ledManager.setColor(LED::YELLOW);
    LOG_INFO("LED set to YELLOW for wait cycle");
}

void handleWaitCycle() {
    // Ensure LED is yellow during wait cycle
    ledManager.setColor(LED::YELLOW);
    LOG_DEBUG("In wait cycle, LED set to YELLOW");
    // State transition is handled in main loop
}

void handleError(const char* errorMessage) {
    LOG_ERROR_F("Error occurred: %s", errorMessage);
    ledManager.indicateError();  // Red LED for errors
    
    // Try to report error via MQTT if possible (with timeouts to prevent hanging)
    bool mqttReportAttempted = false;
    
    if (!networkManager.isWiFiConnected()) {
        // Try WiFi connection with shorter timeout when in error state
        networkManager.connectWiFi();
    }
    
    if (networkManager.isWiFiConnected()) {
        if (!timeManager.isSynced()) {
            timeManager.syncWithNTP();
        }
        
        if (!networkManager.isMQTTConnected()) {
            networkManager.connectMQTT();
        }
        
        if (networkManager.isMQTTConnected()) {
            networkManager.publishStatus(errorMessage);
            String timestamp = timeManager.getCurrentTimestamp();
            networkManager.publishLastUpdate(timestamp.c_str());
            mqttReportAttempted = true;
        }
    }
    
    // Show error for a moment before cleanup
    delay(mqttReportAttempted ? 1000 : 2000);
    
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
