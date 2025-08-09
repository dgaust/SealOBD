#ifndef OBD_MANAGER_H
#define OBD_MANAGER_H

#include <Arduino.h>
#include "ELMduino.h"
#include "BLEClientSerial.h"
#include "Config.h"
#include "Logger.h"

struct VehicleData {
    float stateOfCharge = 0.0;
    float batteryTemperature = 0.0;
    float batteryVoltage = 0.0;
    float totalCharges = 0.0;
    float totalKwhCharged = 0.0;
    float totalKwhDischarged = 0.0;
    bool isValid = false;
};

class OBDManager {
public:
    OBDManager();
    ~OBDManager();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return connected; }
    
    bool readStateOfCharge(float& soc);
    bool readBatteryTemperature(float& temp);
    bool readBatteryVoltage(float& voltage);
    bool readAllData(VehicleData& data);
    bool readTotalCharges(float& charges);
    bool readTotalKwhCharged(float& kwh);
    bool readTotalKwhDischarged(float& kwh);
    
    int getConsecutiveTimeouts() const { return consecutiveTimeouts; }
    bool isCarConnectionLost() const { return carConnectionLost; }
    void resetTimeoutCounter();
    
private:
    BLEClientSerial bleSerial;
    ELM327 elm327;
    bool connected;
    int consecutiveTimeouts;
    bool carConnectionLost;
    
    bool initializeELM327();
    bool sendCommandWithTimeout(const char* command, unsigned long timeout);
    uint8_t charToInt(uint8_t value);
    void handleTimeout(const char* errorMsg);
};

#endif // OBD_MANAGER_H