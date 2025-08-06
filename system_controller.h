// SystemController.h
#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

#include "OBDManager.h"
#include "ConnectivityManager.h"
#include "DataPublisher.h"

class SystemController {
public:
    enum SystemState {
        INIT,
        OBD_READING,
        CONNECTIVITY_SETUP,
        DATA_PUBLISHING,
        ERROR_HANDLING,
        WAITING,
        COMPLETE_CYCLE
    };

    struct Config {
        ConnectivityManager::Config connectivity;
        DataPublisher::Topics topics;
        unsigned long normal_cycle_interval = 300000; // 5 minutes
        unsigned long error_cycle_interval = 60000;   // 1 minute
        unsigned long retry_cycle_interval = 60000;   // 1 minute
    };

    SystemController(const Config& config);
    ~SystemController();
    
    void initialize();
    void update();
    
    SystemState getCurrentState() const { return current_state; }
    const char* getStateString() const;
    const char* getLastError() const { return last_error; }
    
    // Status information
    bool isOBDConnected() const { return obd_manager_.isConnected(); }
    bool isWiFiConnected() const { return connectivity_manager_.isWiFiConnected(); }
    bool isMQTTConnected() const { return connectivity_manager_.isMQTTConnected(); }
    bool isTimeSynced() const { return connectivity_manager_.isTimeSynced(); }
    
private:
    Config config_;
    OBDManager obd_manager_;
    ConnectivityManager connectivity_manager_;
    DataPublisher data_publisher_;
    
    SystemState current_state = INIT;
    SystemState next_state = INIT;
    
    unsigned long last_update_time = 0;
    unsigned long current_cycle_interval = 0;
    unsigned long state_start_time = 0;
    
    char last_error[128];
    bool cycle_successful = false;
    
    // State handling methods
    void handleInit();
    void handleOBDReading();
    void handleConnectivitySetup();
    void handleDataPublishing();
    void handleErrorHandling();
    void handleWaiting();
    void handleCompleteCycle();
    
    // Helper methods
    void setState(SystemState new_state);
    void setError(const char* error);
    void cleanupConnections();
    void setCycleInterval(unsigned long interval);
    
    // Timeout constants
    static const unsigned long CONNECTIVITY_TIMEOUT = 30000; // 30 seconds
    static const unsigned long PUBLISHING_TIMEOUT = 15000;   // 15 seconds
};

#endif