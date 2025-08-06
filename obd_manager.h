// OBDManager.h
#ifndef OBD_MANAGER_H
#define OBD_MANAGER_H

#include "ELMduino.h"
#include "BLEClientSerial.h"

class OBDManager {
public:
    struct OBDData {
        float state_of_charge = 0.0;
        float battery_temperature = 0.0;
        float battery_voltage = 0.0;
        bool valid = false;
    };

    enum OBDState {
        SETUP,
        READ_SOC,
        READ_BATTERY_TEMP,
        READ_BATTERY_VOLTAGE,
        COMPLETE,
        ERROR
    };

    OBDManager();
    ~OBDManager();
    
    bool initialize();
    void disconnect();
    bool isConnected() const { return obd_connected; }
    
    void update();
    OBDState getCurrentState() const { return current_state; }
    const OBDData& getData() const { return data; }
    const char* getLastError() const { return last_error; }
    
    void resetTimeoutCounter();
    bool hasConnectionLost() const { return car_connection_lost; }
    
private:
    enum SetupSubstep {
        SETUP_START,
        BLE_CONNECTING,
        ELM_INITIALIZING
    };
    
    enum QueryState {
        SEND_COMMAND,
        WAITING_RESP
    };
    
    BLEClientSerial ble_serial;
    ELM327 elm327;
    
    OBDState current_state = SETUP;
    SetupSubstep setup_substep = SETUP_START;
    QueryState soc_query_state = SEND_COMMAND;
    QueryState temp_query_state = SEND_COMMAND;
    QueryState voltage_query_state = SEND_COMMAND;
    
    OBDData data;
    bool obd_connected = false;
    bool car_connection_lost = false;
    
    // Timeout tracking
    int consecutive_bt_timeouts = 0;
    static const int MAX_CONSECUTIVE_BT_TIMEOUTS = 2;
    
    // Timing variables
    unsigned long elm_connection_start_time = 0;
    unsigned long elm_soc_start_time = 0;
    unsigned long elm_temp_start_time = 0;
    unsigned long elm_voltage_start_time = 0;
    
    // Timeout constants
    static const unsigned long ELM_CONNECTION_TIMEOUT = 30000;
    static const unsigned long ELM_INIT_TIMEOUT = 15000;
    static const unsigned long ELM_SOC_TIMEOUT = 10000;
    static const unsigned long ELM_TEMP_TIMEOUT = 10000;
    static const unsigned long ELM_VOLTAGE_TIMEOUT = 10000;
    
    char last_error[64];
    
    // Helper methods
    void configureELM327();
    bool handleSetupStep();
    bool handleSOCReading();
    bool handleTempReading();
    bool handleVoltageReading();
    void handleTimeout(const char* error_message);
    bool isBluetoothTimeoutError(const char* error_message);
    uint8_t ctoi(uint8_t value);
};

#endif