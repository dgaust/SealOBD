#include "Logger.h"
#include <stdarg.h>

LogLevel Logger::currentLevel = LogLevel::INFO;

void Logger::begin(unsigned long baudRate) {
    DEBUG_PORT.begin(baudRate);
    while (!DEBUG_PORT) {
        ; // Wait for serial port to connect
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::log(LogLevel level, const char* message) {
    if (level < currentLevel) return;
    
    DEBUG_PORT.print("[");
    DEBUG_PORT.print(millis());
    DEBUG_PORT.print("] [");
    DEBUG_PORT.print(levelToString(level));
    DEBUG_PORT.print("] ");
    DEBUG_PORT.println(message);
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::debug(const char* message) {
    log(LogLevel::DEBUG, message);
}

void Logger::debug(const String& message) {
    debug(message.c_str());
}

void Logger::debugf(const char* format, ...) {
    if (LogLevel::DEBUG < currentLevel) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::DEBUG, buffer);
}

void Logger::info(const char* message) {
    log(LogLevel::INFO, message);
}

void Logger::info(const String& message) {
    info(message.c_str());
}

void Logger::infof(const char* format, ...) {
    if (LogLevel::INFO < currentLevel) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::INFO, buffer);
}

void Logger::warning(const char* message) {
    log(LogLevel::WARNING, message);
}

void Logger::warning(const String& message) {
    warning(message.c_str());
}

void Logger::warningf(const char* format, ...) {
    if (LogLevel::WARNING < currentLevel) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::WARNING, buffer);
}

void Logger::error(const char* message) {
    log(LogLevel::ERROR, message);
}

void Logger::error(const String& message) {
    error(message.c_str());
}

void Logger::errorf(const char* format, ...) {
    if (LogLevel::ERROR < currentLevel) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::ERROR, buffer);
}