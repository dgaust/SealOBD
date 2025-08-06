// DataPublisher.h
#ifndef DATA_PUBLISHER_H
#define DATA_PUBLISHER_H

#include "ConnectivityManager.h"
#include "OBDManager.h"

class DataPublisher {
public:
    struct Topics {
        const char* soc = "bydseal/soc";
        const char* battery_temp = "bydseal/battery_temp";
        const char* battery_voltage = "bydseal/battery_voltage";
        const char* status = "bydseal/status";
        const char* last_update = "bydseal/last_update";
    };

    DataPublisher(ConnectivityManager& connectivity, const Topics& topics = Topics{});
    
    bool publishOBDData(const OBDManager::OBDData& data);
    bool publishStatus(const char* status);
    bool publishLastUpdate();
    bool publishErrorStatus(const char* error_message, bool car_connection_lost);
    
    const char* getLastError() const { return last_error; }
    
private:
    ConnectivityManager& connectivity_;
    Topics topics_;
    char last_error[128];
    
    void setError(const char* error);
    void floatToString(float value, char* buffer, int decimal_places);
};

#endif