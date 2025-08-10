#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <M5AtomS3.h>  // Use M5Stack library instead of raw WS2812B
#include "Config.h"
#include "Logger.h"

// LED Configuration
namespace LED {
    constexpr bool ENABLED = LED_ENABLED;      // Enable/disable from Config.h
    constexpr uint8_t BRIGHTNESS = LED_BRIGHTNESS; // Default brightness from Config.h
    
    // Color definitions (24-bit RGB hex values for M5AtomS3)
    constexpr uint32_t OFF = 0x000000;         // LED off
    constexpr uint32_t RED = 0xFF0000;         // Red for error state
    constexpr uint32_t GREEN = 0x00FF00;       // Green for OBD reading
    constexpr uint32_t BLUE = 0x0000FF;        // Blue for network operations
    constexpr uint32_t YELLOW = 0xFFFF00;      // Yellow for waiting
    constexpr uint32_t PURPLE = 0x800080;      // Purple for setup
    constexpr uint32_t WHITE = 0xFFFFFF;       // White (for debugging)
}

enum class LEDStatus {
    OFF,
    WAITING,        // Yellow - waiting for next update cycle
    OBD_READING,    // Green - reading data from car
    NETWORK_OP,     // Blue - WiFi/MQTT/NTP operations
    ERROR,          // Red - error occurred
    SETUP           // Purple - initial setup/connecting
};

class LEDManager {
public:
    LEDManager();
    ~LEDManager();
    
    // Core LED functions
    void begin();
    void end();
    void setStatus(LEDStatus status);
    void setColor(uint32_t color);
    void setBrightness(uint8_t brightness);
    void turnOff();
    
    // Utility functions
    void blink(uint32_t color, int count = 3, int delay_ms = 200);
    void update(); // Call this in main loop (does nothing now - kept for compatibility)
    
    // Status indication helpers
    void indicateWaiting() { setStatus(LEDStatus::WAITING); }
    void indicateOBDReading() { setStatus(LEDStatus::OBD_READING); }
    void indicateNetworkOperation() { setStatus(LEDStatus::NETWORK_OP); }
    void indicateError() { setStatus(LEDStatus::ERROR); }
    void indicateSetup() { setStatus(LEDStatus::SETUP); }
    
private:
    bool initialized;
    uint8_t currentBrightness;
    LEDStatus currentStatus;
    uint32_t currentColor;
    
    void writeColor(uint32_t color);
    uint32_t applyBrightness(uint32_t color);
    uint32_t getStatusColor(LEDStatus status);
};

#endif // LED_MANAGER_H