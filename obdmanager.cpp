#include "OBDManager.h"

OBDManager::OBDManager() 
    : connected(false), consecutiveTimeouts(0), carConnectionLost(false) {
}

OBDManager::~OBDManager() {
    if (connected) {
        disconnect();
    }
}

bool OBDManager::connect() {
    LOG_INFO("Starting OBD connection...");
    
    // Initialize BLE
    bleSerial.begin(const_cast<char*>(OBD::DEVICE_NAME));
    
    // Connect with timeout
    LOG_INFO("Attempting BLE connection...");
    if (!bleSerial.connect(Timeouts::BLE_CONNECTION)) {
        LOG_ERROR("BLE connection failed or timed out");
        handleTimeout(ErrorMessages::BLE_TIMEOUT);
        return false;
    }
    
    LOG_INFO("BLE connected, initializing ELM327...");
    
    // Initialize ELM327
    if (!initializeELM327()) {
        LOG_ERROR("ELM327 initialization failed");
        handleTimeout(ErrorMessages::INIT_TIMEOUT);
        return false;
    }
    
    connected = true;
    LOG_INFO("OBD connection established successfully");
    return true;
}

bool OBDManager::initializeELM327() {
    unsigned long startTime = millis();
    
    while (!elm327.begin(bleSerial, true, 2000)) {
        if (millis() - startTime > Timeouts::ELM_INIT) {
            return false;
        }
        LOG_DEBUG("Waiting for ELM327 initialization...");
        delay(1000);
    }
    
    LOG_INFO("ELM327 connected, sending initialization commands...");
    
    // Send initialization commands
    for (int i = 0; i < OBD::INIT_COMMANDS_COUNT; i++) {
        LOG_DEBUG_F("Sending: %s", OBD::INIT_COMMANDS[i]);
        elm327.sendCommand_Blocking(OBD::INIT_COMMANDS[i]);
        delay(100);
    }
    
    LOG_INFO("ELM327 initialization complete");
    return true;
}

void OBDManager::disconnect() {
    if (connected) {
        LOG_INFO("Disconnecting OBD...");
        elm327.sendCommand_Blocking("ATZ");
        bleSerial.end();
        connected = false;
        LOG_INFO("OBD disconnected");
    }
}

bool OBDManager::readStateOfCharge(float& soc) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading State of Charge...");
    
    elm327.sendCommand(OBD::CMD_SOC);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("SoC read timeout");
            handleTimeout(ErrorMessages::SOC_TIMEOUT);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        int B = (charToInt(elm327.payload[13]) << 4) | charToInt(elm327.payload[14]);
        soc = float(A + B * 256) / 100.0;
        
        LOG_INFO_F("State of Charge: %.2f%%", soc);
        resetTimeoutCounter();
        return true;
    }
    
    LOG_ERROR("SoC read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::SOC_FAILED);
    return false;
}

bool OBDManager::readBatteryTemperature(float& temp) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading Battery Temperature...");
    
    elm327.sendCommand(OBD::CMD_TEMP);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("Temperature read timeout");
            handleTimeout(ErrorMessages::TEMP_TIMEOUT);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        temp = float(A) - 40.0;
        
        LOG_INFO_F("Battery Temperature: %.1f°C", temp);
        return true;
    }
    
    LOG_ERROR("Temperature read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::TEMP_FAILED);
    return false;
}

bool OBDManager::readBatteryVoltage(float& voltage) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading Battery Voltage...");
    
    elm327.sendCommand(OBD::CMD_VOLTAGE);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("Voltage read timeout");
            handleTimeout(ErrorMessages::VOLTAGE_TIMEOUT);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        int B = (charToInt(elm327.payload[13]) << 4) | charToInt(elm327.payload[14]);
        voltage = float(A + B * 256);
        
        LOG_INFO_F("Battery Voltage: %.2fV", voltage);
        return true;
    }
    
    LOG_ERROR("Voltage read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::VOLTAGE_FAILED);
    return false;
}

bool OBDManager::readTotalCharges(float& charges) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading total charges...");
    
    elm327.sendCommand(OBD::CMD_TOTALCHARGES);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("Total charges read timeout");
            handleTimeout(ErrorMessages::TIMES_CHARGED_FAILED);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        int B = (charToInt(elm327.payload[13]) << 4) | charToInt(elm327.payload[14]);
        charges = float(A + B * 256);
        
        LOG_INFO_F("Total Charges: %.0f", charges);
        resetTimeoutCounter();
        return true;
    }
    
    LOG_ERROR("Total charges read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::TIMES_CHARGED_FAILED);
    return false;
}

bool OBDManager::readTotalKwhCharged(float& kwh) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading total kWh charged...");
    
    elm327.sendCommand(OBD::CMD_TOTALKWHCHARGE);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("Total kWh charged read timeout");
            handleTimeout(ErrorMessages::TOTAL_KWH_CHARGED_FAILED);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        int B = (charToInt(elm327.payload[13]) << 4) | charToInt(elm327.payload[14]);
        kwh = float(A + B * 256);
        
        LOG_INFO_F("Total kWh Charged: %.2f kWh", kwh);
        return true;
    }
    
    LOG_ERROR("Total kWh charged read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::TOTAL_KWH_CHARGED_FAILED);
    return false;
}

bool OBDManager::readTotalKwhDischarged(float& kwh) {
    if (!connected) return false;
    
    LOG_DEBUG("Reading total kWh discharged...");
    
    elm327.sendCommand(OBD::CMD_TOTALKWHDISCHARGE);
    unsigned long startTime = millis();
    
    while (elm327.nb_rx_state == ELM_GETTING_MSG) {
        if (millis() - startTime > Timeouts::ELM_COMMAND) {
            LOG_ERROR("Total kWh discharged read timeout");
            handleTimeout(ErrorMessages::TOTAL_KWH_DISCHARGED_FAILED);
            return false;
        }
        elm327.get_response();
        delay(50);
    }
    
    if (elm327.nb_rx_state == ELM_SUCCESS) {
        int A = (charToInt(elm327.payload[11]) << 4) | charToInt(elm327.payload[12]);
        int B = (charToInt(elm327.payload[13]) << 4) | charToInt(elm327.payload[14]);
        kwh = float(A + B * 256);
        
        LOG_INFO_F("Total kWh Discharged: %.2f kWh", kwh);
        return true;
    }
    
    LOG_ERROR("Total kWh discharged read failed");
    elm327.printError();
    handleTimeout(ErrorMessages::TOTAL_KWH_DISCHARGED_FAILED);
    return false;
}

bool OBDManager::readAllData(VehicleData& data) {
    data.isValid = false;
    
    if (!readStateOfCharge(data.stateOfCharge)) {
        return false;
    }
    
    if (!readBatteryTemperature(data.batteryTemperature)) {
        return false;
    }
    
    if (!readBatteryVoltage(data.batteryVoltage)) {
        return false;
    }
    
    if (!readTotalCharges(data.totalCharges)) {
        return false;
    }
    
    if (!readTotalKwhCharged(data.totalKwhCharged)) {
        return false;
    }
    
    if (!readTotalKwhDischarged(data.totalKwhDischarged)) {
        return false;
    }

    data.isValid = true;
    return true;
}

uint8_t OBDManager::charToInt(uint8_t value) {
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    else if (value >= '0' && value <= '9')
        return value - '0';
    else
        return 0;
}

void OBDManager::handleTimeout(const char* errorMsg) {
    LOG_ERROR_F("OBD timeout: %s", errorMsg);
    
    // Check if it's a Bluetooth-related timeout
    if (strstr(errorMsg, "TIMEOUT") != nullptr) {
        consecutiveTimeouts++;
        LOG_WARNING_F("Consecutive timeouts: %d", consecutiveTimeouts);
        
        if (consecutiveTimeouts >= OBD::MAX_BT_TIMEOUTS) {
            carConnectionLost = true;
            LOG_ERROR("Maximum consecutive timeouts reached - Car connection lost");
        }
    }
}

void OBDManager::resetTimeoutCounter() {
    if (consecutiveTimeouts > 0) {
        LOG_DEBUG_F("Resetting timeout counter from %d", consecutiveTimeouts);
        consecutiveTimeouts = 0;
        carConnectionLost = false;
    }
}