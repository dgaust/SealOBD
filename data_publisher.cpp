// DataPublisher.cpp
#include "DataPublisher.h"
#include <Arduino.h>

DataPublisher::DataPublisher(ConnectivityManager& connectivity, const Topics& topics)
    : connectivity_(connectivity), topics_(topics) {
    strcpy(last_error, "");
}

void DataPublisher::setError(const char* error) {
    strncpy(last_error, error, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    Serial.println(error);
}

void DataPublisher::floatToString(float value, char* buffer, int decimal_places) {
    dtostrf(value, 1, decimal_places, buffer);
}

bool DataPublisher::publishStatus(const char* status) {
    Serial.print("Publishing status: ");
    Serial.println(status);
    
    if (!connectivity_.publishWithTimeout(topics_.status, status)) {
        setError("Failed to publish status");
        return false;
    }
    return true;
}

bool DataPublisher::publishLastUpdate() {
    char datetime_str[64];
    connectivity_.getCurrentTimeString(datetime_str, sizeof(datetime_str));
    
    if (!connectivity_.publishWithTimeout(topics_.last_update, datetime_str)) {
        setError("Failed to publish last update time");
        return false;
    }
    return true;
}

bool DataPublisher::publishOBDData(const OBDManager::OBDData& data) {
    if (!data.valid) {
        setError("OBD data is not valid");
        return false;
    }
    
    bool success = true;
    char value_str[16];
    
    // Publish State of Charge
    floatToString(data.state_of_charge, value_str, 2);
    if (!connectivity_.publishWithTimeout(topics_.soc, value_str)) {
        setError("Failed to publish SoC");
        success = false;
    }
    
    // Publish Battery Temperature
    floatToString(data.battery_temperature, value_str, 1);
    if (!connectivity_.publishWithTimeout(topics_.battery_temp, value_str)) {
        setError("Failed to publish battery temperature");
        success = false;
    }
    
    // Publish Battery Voltage
    floatToString(data.battery_voltage, value_str, 2);
    if (!connectivity_.publishWithTimeout(topics_.battery_voltage, value_str)) {
        setError("Failed to publish battery voltage");
        success = false;
    }
    
    return success;
}

bool DataPublisher::publishErrorStatus(const char* error_message, bool car_connection_lost) {
    bool success = true;
    
    // Publish appropriate status
    if (car_connection_lost) {
        if (!publishStatus("No Car Connection")) {
            success = false;
        }
    } else if (error_message && strlen(error_message) > 0) {
        if (!publishStatus(error_message)) {
            success = false;
        }
    } else {
        if (!publishStatus("UNKNOWN_ERROR")) {
            success = false;
        }
    }
    
    // Always try to publish last update time
    if (!publishLastUpdate()) {
        success = false;
    }
    
    return success;
}