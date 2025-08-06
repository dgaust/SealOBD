// main.ino - Refactored main file using modular libraries
#include <M5Unified.h>
#include "SystemController.h"
#include "arduino_secrets.h"

// Global system controller
SystemController* system_controller = nullptr;

void setup() {
    Serial.begin(115200);
    Serial.println("=== BYD Seal OBD Monitor Starting ===");
    Serial.println("Modular architecture with separate libraries");
    
    // Configure the system
    SystemController::Config config;
    
    // Connectivity configuration
    config.connectivity.ssid = SECRET_SSID;
    config.connectivity.password = SECRET_PASS;
    config.connectivity.mqtt_broker = SECRET_MQTT_IP;
    config.connectivity.mqtt_port = SECRET_MQTT_PORT;
    config.connectivity.mqtt_user = SECRET_MQTT_USER;
    config.connectivity.mqtt_pass = SECRET_MQTT_PASS;
    
    // MQTT Topics (using defaults, but you can customize)
    config.topics.soc = "bydseal/soc";
    config.topics.battery_temp = "bydseal/battery_temp";
    config.topics.battery_voltage = "bydseal/battery_voltage";
    config.topics.status = "bydseal/status";
    config.topics.last_update = "bydseal/last_update";
    
    // Timing configuration
    config.normal_cycle_interval = 300000;  // 5 minutes for successful cycles
    config.error_cycle_interval = 60000;    // 1 minute when no car connection
    config.retry_cycle_interval = 60000;    // 1 minute for retry after errors
    
    // Create and initialize the system controller
    system_controller = new SystemController(config);
    system_controller->initialize();
    
    Serial.println("=== System Initialized ===");
}

void loop() {
    if (system_controller) {
        system_controller->update();
        
        // Optional: Print status information periodically
        static unsigned long last_status_print = 0;
        if (millis() - last_status_print > 10000) { // Every 10 seconds
            Serial.print("System State: ");
            Serial.println(system_controller->getStateString());
            
            if (strlen(system_controller->getLastError()) > 0) {
                Serial.print("Last Error: ");
                Serial.println(system_controller->getLastError());
            }
            
            last_status_print = millis();
        }
    }
    
    // Small delay to prevent excessive CPU usage
    delay(100);
}