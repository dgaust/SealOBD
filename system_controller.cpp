// SystemController.cpp
#include "SystemController.h"
#include <Arduino.h>

SystemController::SystemController(const Config& config)
    : config_(config)
    , connectivity_manager_(config.connectivity)
    , data_publisher_(connectivity_manager_, config.topics) {
    strcpy(last_error, "");
}

SystemController::~SystemController() {
    cleanupConnections();
}

void SystemController::setError(const char* error) {
    strncpy(last_error, error, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    Serial.println(error);
}

void SystemController::setState(SystemState new_state) {
    if (current_state != new_state) {
        Serial.print("State transition: ");
        Serial.print(getStateString());
        Serial.print(" -> ");
        current_state = new_state;
        Serial.println(getStateString());
        state_start_time = millis();
    }
}

const char* SystemController::getStateString() const {
    switch (current_state) {
        case INIT: return "INIT";
        case OBD_READING: return "OBD_READING";
        case CONNECTIVITY_SETUP: return "CONNECTIVITY_SETUP";
        case DATA_PUBLISHING: return "DATA_PUBLISHING";
        case ERROR_HANDLING: return "ERROR_HANDLING";
        case WAITING: return "WAITING";
        case COMPLETE_CYCLE: return "COMPLETE_CYCLE";
        default: return "UNKNOWN";
    }
}

void SystemController::cleanupConnections() {
    connectivity_manager_.disconnectMQTT();
    connectivity_manager_.disconnectWiFi();
    obd_manager_.disconnect();
}

void SystemController::setCycleInterval(unsigned long interval) {
    current_cycle_interval = interval;
    last_update_time = millis();
}

void SystemController::initialize() {
    Serial.println("Initializing System Controller...");
    setState(INIT);
    setCycleInterval(0); // Start immediately
}

void SystemController::handleInit() {
    Serial.println("=== Starting New Cycle ===");
    cycle_successful = false;
    
    if (!obd_manager_.initialize()) {
        setError("Failed to initialize OBD manager");
        setState(ERROR_HANDLING);
        return;
    }
    
    setState(OBD_READING);
}

void SystemController::handleOBDReading() {
    obd_manager_.update();
    
    switch (obd_manager_.getCurrentState()) {
        case OBDManager::COMPLETE:
            Serial.println("OBD reading complete successfully");
            setState(CONNECTIVITY_SETUP);
            break;
            
        case OBDManager::ERROR:
            setError(obd_manager_.getLastError());
            setState(ERROR_HANDLING);
            break;
            
        default:
            // Still in progress, continue updating
            break;
    }
}

void SystemController::handleConnectivitySetup() {
    static enum {
        SETUP_WIFI,
        SETUP_NTP,
        SETUP_MQTT,
        CONNECTIVITY_COMPLETE
    } connectivity_step = SETUP_WIFI;
    
    // Check for timeout
    if (millis() - state_start_time > CONNECTIVITY_TIMEOUT) {
        setError("Connectivity setup timeout");
        setState(ERROR_HANDLING);
        connectivity_step = SETUP_WIFI; // Reset for next time
        return;
    }
    
    switch (connectivity_step) {
        case SETUP_WIFI:
            Serial.println("Setting up WiFi...");
            if (connectivity_manager_.connectWiFi()) {
                connectivity_step = SETUP_NTP;
            } else {
                setError("WiFi connection failed");
                setState(ERROR_HANDLING);
                connectivity_step = SETUP_WIFI;
                return;
            }
            break;
            
        case SETUP_NTP:
            Serial.println("Synchronizing time...");
            if (connectivity_manager_.syncTime()) {
                connectivity_step = SETUP_MQTT;
            } else {
                Serial.println("NTP sync failed, continuing without accurate time");
                connectivity_step = SETUP_MQTT;
            }
            break;
            
        case SETUP_MQTT:
            Serial.println("Connecting to MQTT...");
            if (connectivity_manager_.connectMQTT()) {
                connectivity_step = CONNECTIVITY_COMPLETE;
            } else {
                setError("MQTT connection failed");
                setState(ERROR_HANDLING);
                connectivity_step = SETUP_WIFI;
                return;
            }
            break;
            
        case CONNECTIVITY_COMPLETE:
            Serial.println("Connectivity setup complete");
            setState(DATA_PUBLISHING);
            connectivity_step = SETUP_WIFI; // Reset for next cycle
            break;
    }
}

void SystemController::handleDataPublishing() {
    // Check for timeout
    if (millis() - state_start_time > PUBLISHING_TIMEOUT) {
        setError("Data publishing timeout");
        setState(ERROR_HANDLING);
        return;
    }
    
    bool success = true;
    
    // Publish status first
    if (obd_manager_.hasConnectionLost()) {
        success &= data_publisher_.publishStatus("No Car Connection");
    } else {
        success &= data_publisher_.publishStatus("CONNECTED");
        
        // Only publish OBD data if we have a car connection
        const OBDManager::OBDData& data = obd_manager_.getData();
        success &= data_publisher_.publishOBDData(data);
    }
    
    // Always publish last update time
    success &= data_publisher_.publishLastUpdate();
    
    if (success) {
        Serial.println("Data publishing complete");
        cycle_successful = true;
    } else {
        Serial.println("Data publishing had errors, but continuing");
        // Don't mark as error state since some data may have been published
        cycle_successful = false;
    }
    
    setState(COMPLETE_CYCLE);
}

void SystemController::handleErrorHandling() {
    Serial.println("Handling error condition...");
    
    // Try to publish error status if we can establish connectivity
    bool error_published = false;
    
    if (!connectivity_manager_.isWiFiConnected()) {
        if (connectivity_manager_.connectWiFi()) {
            connectivity_manager_.syncTime(); // Best effort
        }
    }
    
    if (connectivity_manager_.isWiFiConnected() && !connectivity_manager_.isMQTTConnected()) {
        connectivity_manager_.connectMQTT();
    }
    
    if (connectivity_manager_.isMQTTConnected()) {
        error_published = data_publisher_.publishErrorStatus(
            last_error, 
            obd_manager_.hasConnectionLost()
        );
    }
    
    if (error_published) {
        Serial.println("Error status published successfully");
    } else {
        Serial.println("Could not publish error status");
    }
    
    setState(COMPLETE_CYCLE);
}

void SystemController::handleCompleteCycle() {
    Serial.println("=== Cycle Complete ===");
    
    // Cleanup all connections
    cleanupConnections();
    
    // Determine next cycle interval based on success
    if (cycle_successful) {
        setCycleInterval(config_.normal_cycle_interval);
        Serial.print("Successful cycle. Next update in ");
        Serial.print(config_.normal_cycle_interval / 1000);
        Serial.println(" seconds");
    } else if (obd_manager_.hasConnectionLost()) {
        setCycleInterval(config_.error_cycle_interval);
        Serial.print("No car connection. Next update in ");
        Serial.print(config_.error_cycle_interval / 1000);
        Serial.println(" seconds");
    } else {
        setCycleInterval(config_.retry_cycle_interval);
        Serial.print("Error occurred. Next retry in ");
        Serial.print(config_.retry_cycle_interval / 1000);
        Serial.println(" seconds");
    }
    
    setState(WAITING);
}

void SystemController::handleWaiting() {
    // Poll MQTT if connected (shouldn't normally be connected in wait state)
    if (connectivity_manager_.isMQTTConnected()) {
        connectivity_manager_.poll();
    }
    
    // Check if it's time for the next cycle
    if (millis() - last_update_time >= current_cycle_interval) {
        setState(INIT);
    }
}

void SystemController::update() {
    switch (current_state) {
        case INIT:
            handleInit();
            break;
            
        case OBD_READING:
            handleOBDReading();
            break;
            
        case CONNECTIVITY_SETUP:
            handleConnectivitySetup();
            break;
            
        case DATA_PUBLISHING:
            handleDataPublishing();
            break;
            
        case ERROR_HANDLING:
            handleErrorHandling();
            break;
            
        case WAITING:
            handleWaiting();
            break;
            
        case COMPLETE_CYCLE:
            handleCompleteCycle();
            break;
    }
}