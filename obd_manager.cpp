// OBDManager.cpp
#include "OBDManager.h"
#include <Arduino.h>

OBDManager::OBDManager() {
    strcpy(last_error, "");
}

OBDManager::~OBDManager() {
    disconnect();
}

bool OBDManager::initialize() {
    Serial.println("Initializing OBD Manager...");
    current_state = SETUP;
    setup_substep = SETUP_START;
    return true;
}

void OBDManager::disconnect() {
    if (obd_connected) {
        Serial.println("Disconnecting OBD/ELM327...");
        elm327.sendCommand_Blocking("ATZ");
        ble_serial.end();
        obd_connected = false;
        Serial.println("OBD/ELM327 disconnected");
    }
}

uint8_t OBDManager::ctoi(uint8_t value) {
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    else if (value >= '0' && value <= '9')
        return value - '0';
    else
        return 0;
}

void OBDManager::configureELM327() {
    Serial.println("Configuring ELM327...");
    elm327.sendCommand_Blocking("ATZ");
    elm327.sendCommand_Blocking("ATD");
    elm327.sendCommand_Blocking("ATD0");
    elm327.sendCommand_Blocking("ATH1");
    elm327.sendCommand_Blocking("ATSP6");
    elm327.sendCommand_Blocking("ATE0");
    elm327.sendCommand_Blocking("ATH1");
    elm327.sendCommand_Blocking("ATM0");
    elm327.sendCommand_Blocking("ATS0");
    elm327.sendCommand_Blocking("ATAT1");
    elm327.sendCommand_Blocking("ATAL");
    elm327.sendCommand_Blocking("STCSEGT1");
    elm327.sendCommand_Blocking("ATST96");
    elm327.sendCommand_Blocking("ATSH7E7");
}

bool OBDManager::isBluetoothTimeoutError(const char* error_message) {
    return (strstr(error_message, "ELM_BLE_CONNECTION_TIMEOUT") != NULL ||
            strstr(error_message, "ELM_INIT_TIMEOUT") != NULL ||
            strstr(error_message, "SOC_READ_TIMEOUT") != NULL ||
            strstr(error_message, "TEMP_READ_TIMEOUT") != NULL ||
            strstr(error_message, "VOLTAGE_READ_TIMEOUT") != NULL);
}

void OBDManager::handleTimeout(const char* error_message) {
    Serial.print("ELM327 timeout: ");
    Serial.println(error_message);
    
    strcpy(last_error, error_message);
    
    if (isBluetoothTimeoutError(error_message)) {
        consecutive_bt_timeouts++;
        Serial.print("Consecutive Bluetooth timeouts: ");
        Serial.println(consecutive_bt_timeouts);
        
        if (consecutive_bt_timeouts >= MAX_CONSECUTIVE_BT_TIMEOUTS) {
            car_connection_lost = true;
            Serial.println("Maximum consecutive Bluetooth timeouts reached - No Car Connection");
        }
    } else {
        consecutive_bt_timeouts = 0;
        car_connection_lost = false;
    }
    
    disconnect();
    current_state = ERROR;
}

void OBDManager::resetTimeoutCounter() {
    if (consecutive_bt_timeouts > 0) {
        Serial.print("Resetting Bluetooth timeout counter from ");
        Serial.println(consecutive_bt_timeouts);
        consecutive_bt_timeouts = 0;
        car_connection_lost = false;
    }
}

bool OBDManager::handleSetupStep() {
    switch (setup_substep) {
        case SETUP_START:
            Serial.println("Setting up OBD connection...");
            ble_serial.begin("OBDLink CX");
            elm_connection_start_time = millis();
            setup_substep = BLE_CONNECTING;
            break;
            
        case BLE_CONNECTING:
            Serial.println("Attempting BLE connection...");
            if (!ble_serial.connect(15000)) {
                Serial.println("BLE connection failed or timed out");
                handleTimeout("ELM_BLE_CONNECTION_TIMEOUT");
                setup_substep = SETUP_START;
                return false;
            }
            Serial.println("BLE connected, initializing ELM327...");
            elm_connection_start_time = millis();
            setup_substep = ELM_INITIALIZING;
            break;
            
        case ELM_INITIALIZING:
            if (!elm327.begin(ble_serial, true, 2000)) {
                if (millis() - elm_connection_start_time > ELM_INIT_TIMEOUT) {
                    handleTimeout("ELM_INIT_TIMEOUT");
                    setup_substep = SETUP_START;
                    return false;
                }
                Serial.println("Initializing ELM327...");
                return false; // Still in progress
            }
            
            configureELM327();
            obd_connected = true;
            current_state = READ_SOC;
            soc_query_state = SEND_COMMAND;
            setup_substep = SETUP_START;
            Serial.println("OBD setup complete");
            return true;
    }
    return false; // Still in progress
}

bool OBDManager::handleSOCReading() {
    if (soc_query_state == SEND_COMMAND) {
        elm327.sendCommand("221FFC");
        soc_query_state = WAITING_RESP;
        elm_soc_start_time = millis();
        return false;
    } 
    
    if (soc_query_state == WAITING_RESP) {
        if (millis() - elm_soc_start_time > ELM_SOC_TIMEOUT) {
            handleTimeout("SOC_READ_TIMEOUT");
            return false;
        }
        
        elm327.get_response();
        
        if (elm327.nb_rx_state == ELM_SUCCESS) {
            int A = (ctoi(elm327.payload[11]) << 4) | ctoi(elm327.payload[12]);
            int B = (ctoi(elm327.payload[13]) << 4) | ctoi(elm327.payload[14]);
            B = B * 256;
            data.state_of_charge = float(A + B) / 100.0;
            
            Serial.print("State of Charge: ");
            Serial.println(data.state_of_charge, 2);
            
            resetTimeoutCounter();
            soc_query_state = SEND_COMMAND;
            current_state = READ_BATTERY_TEMP;
            temp_query_state = SEND_COMMAND;
            return true;
        }
        else if (elm327.nb_rx_state != ELM_GETTING_MSG) {
            soc_query_state = SEND_COMMAND;
            elm327.printError();
            handleTimeout("SOC_READ_FAILED");
            return false;
        }
    }
    return false; // Still in progress
}

bool OBDManager::handleTempReading() {
    if (temp_query_state == SEND_COMMAND) {
        elm327.sendCommand("220032");
        temp_query_state = WAITING_RESP;
        elm_temp_start_time = millis();
        return false;
    }
    
    if (temp_query_state == WAITING_RESP) {
        if (millis() - elm_temp_start_time > ELM_TEMP_TIMEOUT) {
            handleTimeout("TEMP_READ_TIMEOUT");
            return false;
        }
        
        elm327.get_response();
        
        if (elm327.nb_rx_state == ELM_SUCCESS) {
            int A = (ctoi(elm327.payload[11]) << 4) | ctoi(elm327.payload[12]);
            data.battery_temperature = float(A) - 40.0;
            
            Serial.print("Battery Temperature: ");
            Serial.print(data.battery_temperature, 1);
            Serial.println(" °C");
            
            temp_query_state = SEND_COMMAND;
            current_state = READ_BATTERY_VOLTAGE;
            voltage_query_state = SEND_COMMAND;
            return true;
        }
        else if (elm327.nb_rx_state != ELM_GETTING_MSG) {
            temp_query_state = SEND_COMMAND;
            elm327.printError();
            handleTimeout("TEMP_READ_FAILED");
            return false;
        }
    }
    return false; // Still in progress
}

bool OBDManager::handleVoltageReading() {
    if (voltage_query_state == SEND_COMMAND) {
        elm327.sendCommand("220008");
        voltage_query_state = WAITING_RESP;
        elm_voltage_start_time = millis();
        return false;
    }
    
    if (voltage_query_state == WAITING_RESP) {
        if (millis() - elm_voltage_start_time > ELM_VOLTAGE_TIMEOUT) {
            handleTimeout("VOLTAGE_READ_TIMEOUT");
            return false;
        }
        
        elm327.get_response();
        
        if (elm327.nb_rx_state == ELM_SUCCESS) {
            int A = (ctoi(elm327.payload[11]) << 4) | ctoi(elm327.payload[12]);
            int B = (ctoi(elm327.payload[13]) << 4) | ctoi(elm327.payload[14]);
            data.battery_voltage = float(A + B * 256);
            
            Serial.print("Battery Voltage: ");
            Serial.print(data.battery_voltage, 2);
            Serial.println(" V");
            
            voltage_query_state = SEND_COMMAND;
            current_state = COMPLETE;
            data.valid = true;
            return true;
        }
        else if (elm327.nb_rx_state != ELM_GETTING_MSG) {
            voltage_query_state = SEND_COMMAND;
            elm327.printError();
            handleTimeout("VOLTAGE_READ_FAILED");
            return false;
        }
    }
    return false; // Still in progress
}

void OBDManager::update() {
    switch (current_state) {
        case SETUP:
            handleSetupStep();
            break;
            
        case READ_SOC:
            handleSOCReading();
            break;
            
        case READ_BATTERY_TEMP:
            handleTempReading();
            break;
            
        case READ_BATTERY_VOLTAGE:
            handleVoltageReading();
            break;
            
        case COMPLETE:
        case ERROR:
            // States that don't need continuous updates
            break;
    }
}