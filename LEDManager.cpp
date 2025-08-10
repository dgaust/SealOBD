#include "LEDManager.h"

LEDManager::LEDManager() 
    : initialized(false), currentBrightness(LED::BRIGHTNESS), currentStatus(LEDStatus::OFF),
      currentColor(LED::OFF) {
}

LEDManager::~LEDManager() {
    end();
}

void LEDManager::begin() {
    if (!LED::ENABLED) {
        LOG_DEBUG("LED functionality disabled in config");
        return;
    }
    
    // Initialize M5AtomS3 (this also initializes the LED)
    // Note: AtomS3.begin() should be called in main setup, not here
    // We just mark as initialized if LED is enabled
    
    initialized = true;
    
    // Set initial brightness
    AtomS3.dis.setBrightness(currentBrightness);
    
    // Start with LED off
    turnOff();
    
    LOG_DEBUG("LED Manager initialized for M5Stack AtomS3 Lite");
    
    // Brief startup indication - blue blink
    blink(LED::BLUE, 2, 100);
}

void LEDManager::end() {
    if (initialized && LED::ENABLED) {
        turnOff();
        initialized = false;
        LOG_DEBUG("LED Manager deinitialized");
    }
}

void LEDManager::setStatus(LEDStatus status) {
    if (!LED::ENABLED || !initialized) return;
    
    currentStatus = status;
    uint32_t color = getStatusColor(status);
    setColor(color);
    
    LOG_DEBUG_F("LED status set to: %d (color: 0x%06X)", (int)status, color);
}

void LEDManager::setColor(uint32_t color) {
    if (!LED::ENABLED || !initialized) return;
    
    currentColor = color;
    writeColor(color);
}

void LEDManager::setBrightness(uint8_t brightness) {
    if (!LED::ENABLED || !initialized) return;
    
    currentBrightness = brightness;
    
    // Set M5Stack display brightness (affects LED)
    AtomS3.dis.setBrightness(brightness);
    
    // Reapply current color with new brightness
    writeColor(currentColor);
    
    LOG_DEBUG_F("LED brightness set to: %d", brightness);
}

void LEDManager::turnOff() {
    if (!LED::ENABLED || !initialized) return;
    
    setColor(LED::OFF);
    currentStatus = LEDStatus::OFF;
}

void LEDManager::blink(uint32_t color, int count, int delay_ms) {
    if (!LED::ENABLED || !initialized) return;
    
    for (int i = 0; i < count; i++) {
        setColor(color);
        delay(delay_ms);
        turnOff();
        if (i < count - 1) { // Don't delay after the last blink
            delay(delay_ms);
        }
    }
    
    // Restore previous status color after blinking
    setColor(getStatusColor(currentStatus));
}

void LEDManager::update() {
    // No animations needed - just static LED colors
    // This function can be called but does nothing now
    // Kept for compatibility with existing code
}

void LEDManager::writeColor(uint32_t color) {
    if (!LED::ENABLED || !initialized) return;
    
    // Apply overall brightness scaling
    uint32_t finalColor = applyBrightness(color);
    
    // Use M5Stack AtomS3 LED control
    AtomS3.dis.drawpix(finalColor);
    AtomS3.update();
    
    LOG_DEBUG_F("LED color written: 0x%06X", finalColor);
}

uint32_t LEDManager::applyBrightness(uint32_t color) {
    // Extract RGB components
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    // Apply brightness factor
    r = (r * currentBrightness) / 100;
    g = (g * currentBrightness) / 100;
    b = (b * currentBrightness) / 100;
    
    // Recombine into 24-bit color
    return (r << 16) | (g << 8) | b;
}

uint32_t LEDManager::getStatusColor(LEDStatus status) {
    switch (status) {
        case LEDStatus::WAITING:
            return LED::YELLOW;     // Yellow for waiting
        case LEDStatus::OBD_READING:
            return LED::GREEN;      // Green for OBD reading
        case LEDStatus::NETWORK_OP:
            return LED::BLUE;       // Blue for network operations
        case LEDStatus::ERROR:
            return LED::RED;        // Red for errors
        case LEDStatus::SETUP:
            return LED::PURPLE;     // Purple for setup/connecting
        case LEDStatus::OFF:
        default:
            return LED::OFF;        // LED off
    }
}