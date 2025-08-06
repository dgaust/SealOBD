# BYD Seal OBD Monitor

Reads the current state of charge from a BYD Seal (and possibly others), using an ESP32 conneting to an OBDLink CX using BLE. Publishes the resulting SOC, Battery Voltage and Battery Temp (just testing this value at the moment) to an MQTT server.

Requires the ELMDuino library https://docs.arduino.cc/libraries/elmduino/ 

The code contains a a slightly tweaked BLE Serial library from https://github.com/vdvornichenko/obd-ble-serial

## Architecture Overview

The code has been split into several focused libraries:

### 1. **OBDManager** (`OBDManager.h/.cpp`)
- **Purpose**: Handles all OBD-II/ELM327 communication
- **Responsibilities**:
  - Bluetooth connection to ELM327 device
  - Reading State of Charge (SoC)
  - Reading battery temperature
  - Reading battery voltage  
  - Connection timeout and retry logic
  - Error handling and status tracking

### 2. **ConnectivityManager** (`ConnectivityManager.h/.cpp`)
- **Purpose**: Manages all network connectivity
- **Responsibilities**:
  - WiFi connection and disconnection
  - MQTT broker connection and messaging
  - NTP time synchronization
  - Network timeout handling
  - Connection status monitoring

### 3. **DataPublisher** (`DataPublisher.h/.cpp`)
- **Purpose**: Handles data publishing to MQTT topics
- **Responsibilities**:
  - Publishing OBD data (SoC, temperature, voltage)
  - Publishing system status messages
  - Publishing timestamp information
  - Error status publishing
  - Data formatting

### 4. **SystemController** (`SystemController.h/.cpp`)
- **Purpose**: Orchestrates the entire system workflow
- **Responsibilities**:
  - State machine management
  - Coordinating between all managers
  - Cycle timing and intervals
  - Overall error handling
  - System lifecycle management

### 5. **Main Application** (`main.ino`)
- **Purpose**: System configuration and main loop
- **Responsibilities**:
  - System initialization
  - Configuration setup
  - Main update loop

## File Structure

```
project/
├── main.ino                    # Main application file
├── OBDManager.h               # OBD manager header
├── OBDManager.cpp             # OBD manager implementation
├── ConnectivityManager.h      # Connectivity manager header  
├── ConnectivityManager.cpp    # Connectivity manager implementation
├── DataPublisher.h            # Data publisher header
├── DataPublisher.cpp          # Data publisher implementation
├── SystemController.h         # System controller header
├── SystemController.cpp       # System controller implementation
├── arduino_secrets.h          # Configuration secrets
└── README.md                  # This file
```

## Key Benefits of This Architecture

### **Separation of Concerns**
- Each library has a single, well-defined responsibility
- Easy to understand what each component does
- Changes in one area don't affect others

### **Improved Maintainability**
- Bugs can be isolated to specific libraries
- Updates and improvements can be made incrementally
- Code is more organized and easier to navigate

### **Better Error Handling**
- Each component manages its own errors
- Clear error propagation between components
- More granular error reporting

### **Enhanced Testing**
- Individual components can be tested separately
- Mock objects can be used for unit testing
- Integration testing is more straightforward

### **Reusability**
- Libraries can be reused in other projects
- Components can be easily swapped or upgraded
- Common patterns are abstracted into reusable classes

## State Machine Flow

The `SystemController` manages the overall system state:

1. **INIT** - Initialize all components
2. **OBD_READING** - Read data from vehicle via OBD
3. **CONNECTIVITY_SETUP** - Connect WiFi, sync time, connect MQTT
4. **DATA_PUBLISHING** - Publish all data to MQTT topics
5. **ERROR_HANDLING** - Handle any errors that occurred
6. **COMPLETE_CYCLE** - Clean up connections, set next cycle interval
7. **WAITING** - Wait for next cycle to begin

## Configuration

All configuration is centralized in `main.ino`:

```cpp
SystemController::Config config;

// Network settings
config.connectivity.ssid = SECRET_SSID;
config.connectivity.password = SECRET_PASS;
config.connectivity.mqtt_broker = SECRET_MQTT_IP;
// ... etc

// Timing settings  
config.normal_cycle_interval = 300000;  // 5 minutes
config.error_cycle_interval = 60000;    // 1 minute
```

## Error Handling

The system now has comprehensive error handling:

- **OBD Errors**: Connection timeouts, read failures
- **Network Errors**: WiFi connection issues, MQTT problems
- **Publishing Errors**: Failed message publishing
- **Timeout Protection**: All operations have configurable timeouts

## Memory Management

- Proper constructor/destructor patterns
- Automatic cleanup of connections
- No memory leaks from unclosed connections

## Getting Started

1. Copy all library files to your Arduino project directory
2. Update `arduino_secrets.h` with your credentials
3. Adjust configuration in `main.ino` as needed
4. Upload to your ESP32 device

## Dependencies

This code still requires the same external libraries:
- `WiFi.h` 
- `ArduinoMqttClient.h`
- `M5Unified.h`
- `ELMduino.h`
- `BLEClientSerial.h`

## Future Enhancements

With this modular architecture, you can easily:

- Add new data sources (additional OBD PIDs)
- Implement different connectivity options (cellular, LoRaWAN)
- Add data logging to SD card
- Implement web server for local monitoring
- Add display output for real-time monitoring
- Implement OTA updates

The modular design makes all of these enhancements much more manageable than with the original monolithic code.