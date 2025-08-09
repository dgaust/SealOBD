#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "Config.h"
#include "Logger.h"

class TimeManager {
public:
    TimeManager();
    
    bool syncWithNTP();
    bool isSynced() const { return timeSynced; }
    
    String getCurrentTimestamp();
    void getFormattedTime(char* buffer, size_t bufferSize);
    
private:
    bool timeSynced;
    
    bool waitForSync(int maxRetries);
};

#endif // TIME_MANAGER_H