#include "TimeManager.h"

TimeManager::TimeManager() : timeSynced(false) {
}

bool TimeManager::syncWithNTP() {
    LOG_INFO("Synchronizing time with NTP servers...");
    
    configTime(0, 0, NTP::SERVER1, NTP::SERVER2, NTP::SERVER3);
    setenv("TZ", NTP::TIMEZONE, 1);
    tzset();
    
    if (waitForSync(NTP::MAX_RETRY)) {
        timeSynced = true;
        
        char timeStr[64];
        getFormattedTime(timeStr, sizeof(timeStr));
        LOG_INFO_F("Time synchronized: %s", timeStr);
        return true;
    }
    
    LOG_ERROR("Failed to sync time with NTP");
    return false;
}

bool TimeManager::waitForSync(int maxRetries) {
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    
    while (timeinfo.tm_year < (2024 - 1900) && retry < maxRetries) {
        LOG_DEBUG_F("Waiting for time sync... (%d/%d)", retry + 1, maxRetries);
        delay(1000);
        time(&now);
        localtime_r(&now, &timeinfo);
        retry++;
    }
    
    return timeinfo.tm_year >= (2024 - 1900);
}

String TimeManager::getCurrentTimestamp() {
    if (!timeSynced) {
        return String(ErrorMessages::TIME_NOT_SYNCED);
    }
    
    char buffer[64];
    getFormattedTime(buffer, sizeof(buffer));
    return String(buffer);
}

void TimeManager::getFormattedTime(char* buffer, size_t bufferSize) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
}