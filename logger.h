#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "Config.h"

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void begin(unsigned long baudRate = DEBUG_BAUD_RATE);
    static void setLevel(LogLevel level);
    
    static void debug(const char* message);
    static void debug(const String& message);
    static void debugf(const char* format, ...);
    
    static void info(const char* message);
    static void info(const String& message);
    static void infof(const char* format, ...);
    
    static void warning(const char* message);
    static void warning(const String& message);
    static void warningf(const char* format, ...);
    
    static void error(const char* message);
    static void error(const String& message);
    static void errorf(const char* format, ...);
    
private:
    static LogLevel currentLevel;
    static void log(LogLevel level, const char* message);
    static const char* levelToString(LogLevel level);
};

// Convenience macros
#if DEBUG_ENABLED
    #define LOG_DEBUG(msg) Logger::debug(msg)
    #define LOG_INFO(msg) Logger::info(msg)
    #define LOG_WARNING(msg) Logger::warning(msg)
    #define LOG_ERROR(msg) Logger::error(msg)
    #define LOG_DEBUG_F(...) Logger::debugf(__VA_ARGS__)
    #define LOG_INFO_F(...) Logger::infof(__VA_ARGS__)
    #define LOG_WARNING_F(...) Logger::warningf(__VA_ARGS__)
    #define LOG_ERROR_F(...) Logger::errorf(__VA_ARGS__)
#else
    #define LOG_DEBUG(msg)
    #define LOG_INFO(msg)
    #define LOG_WARNING(msg)
    #define LOG_ERROR(msg)
    #define LOG_DEBUG_F(...)
    #define LOG_INFO_F(...)
    #define LOG_WARNING_F(...)
    #define LOG_ERROR_F(...)
#endif

#endif // LOGGER_H