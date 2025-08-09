# BYD Seal OBD Monitor

**BYD Seal OBD Monitor**

Reads the current state of charge from a BYD Seal (and possibly others), using an ESP32 connecting to an OBDLink CX using BLE.

Requires the ELMDuino library https://docs.arduino.cc/libraries/elmduino/

The code contains a slightly tweaked BLE Serial library from https://github.com/vdvornichenko/obd-ble-serial

## What It Does

This project turns an ESP32 into a car battery monitor for your BYD Seal. It connects to your car through an OBDLink CX adapter using Bluetooth, reads important battery information, and sends it to your home automation system using MQTT.

### Information It Collects
- **Battery State of Charge** - How full your battery is (as a percentage)
- **Battery Temperature** - How hot or cold your battery pack is
- **Battery Voltage** - The current voltage of your battery
- **Total Charges** - How many times the car has been charged
- **Total kWh Charged** - Total energy put into the battery over its lifetime
- **Total kWh Discharged** - Total energy taken out of the battery over its lifetime

## What You Need

### Hardware
- **ESP32 board** - This has only been tested on the [M5Stack AtomS3 Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite), but other ESP32 boards should work with minor modifications
- **OBDLink CX** - A Bluetooth OBD adapter that plugs into your car
- **BYD Seal** or similar BYD electric vehicle

### Software Libraries
- Arduino IDE
- ELMDuino library
- ArduinoMqttClient library
- M5Unified library (required for M5Stack AtomS3 Lite)

## How to Set It Up

### 1. Configure Your Settings
Create a file called `arduino_secrets.h` and add your network details:

```cpp
#define SECRET_SSID "Your_WiFi_Name"
#define SECRET_PASS "Your_WiFi_Password"
#define SECRET_MQTT_USER "mqtt_username"
#define SECRET_MQTT_PASS "mqtt_password"
#define SECRET_MQTT_IP "192.168.1.100"  // Your MQTT broker IP
#define SECRET_MQTT_PORT 1883
```

### 2. Install the Code
1. Open Arduino IDE
2. Install the required libraries through the Library Manager
3. Copy all the project files to your Arduino project folder
4. Upload the code to your ESP32

### 3. Connect to Your Car
1. Plug the OBDLink CX into your car's OBD port (usually under the dashboard)
2. Power on the ESP32
3. The ESP32 will automatically find and connect to the OBDLink CX

## How It Works

The monitor follows these steps every 5 minutes:

1. **Connect to Car** - Establishes Bluetooth connection to OBDLink CX
2. **Read Battery Data** - Gets all the battery information from your car
3. **Connect to WiFi** - Joins your home network
4. **Sync Time** - Gets the current time from internet time servers
5. **Send Data** - Publishes all the information to your MQTT broker
6. **Sleep** - Waits 5 minutes before doing it all again

If something goes wrong, it will retry after 1 minute instead of 5.

## MQTT Topics

The monitor publishes data to these MQTT topics:

- `bydseal/soc` - Battery charge percentage
- `bydseal/battery_temp` - Battery temperature in Celsius
- `bydseal/battery_voltage` - Battery voltage
- `bydseal/total_charges` - Number of times charged
- `bydseal/kwh_charged` - Total kWh charged
- `bydseal/kwh_discharged` - Total kWh used
- `bydseal/status` - Current status (Connected or error message)
- `bydseal/last_update` - When the last update happened

## Understanding the Files

- **sealobd.ino** - The main program that runs everything
- **Config.h** - All the settings and options
- **OBDManager** - Handles talking to your car
- **MQTTNetworkManager** - Handles WiFi and sending data
- **TimeManager** - Keeps track of time
- **Logger** - Shows what's happening (for debugging)
- **BLEClientSerial** - Bluetooth communication with OBDLink

## Troubleshooting

### Common Issues

**Can't connect to OBDLink CX**
- Make sure the OBDLink CX is plugged into your car
- Check that your car is turned on
- The adapter name should be "OBDLink CX"

**No data being read**
- Your car needs to be in "Ready" mode
- Wait a few seconds after turning on the car
- Check the serial monitor for error messages

**MQTT not working**
- Verify your MQTT broker IP address is correct
- Check username and password
- Make sure MQTT broker is running

**WiFi won't connect**
- Double-check your WiFi name and password
- Make sure the ESP32 is in range of your router

## Customization

### Change Update Frequency
In `Config.h`, modify these values (in milliseconds):
- `NORMAL_UPDATE = 300000` - Normal update every 5 minutes
- `ERROR_RETRY = 60000` - Retry after error every 1 minute

### Change MQTT Topics
Edit the topic names in `Config.h` under the MQTT namespace.

### Adjust Timeouts
If connections are timing out, increase the timeout values in `Config.h`.

## Safety Notes

- The monitor only reads data from your car - it doesn't change anything
- Always plug the OBDLink CX in carefully to avoid damaging the port
- The system is designed to handle connection losses gracefully

## How to Use the Data

The MQTT data can be used with:
- **Home Assistant** - Create sensors and automations
- **Node-RED** - Build flows and dashboards
- **Grafana** - Create beautiful graphs of your battery usage
- **Any MQTT client** - Subscribe to topics and use the data

## Tested Hardware

This project has been developed and tested specifically on the **M5Stack AtomS3 Lite**. While the code should work on other ESP32 boards, you may need to:
- Remove or modify the M5Unified library references
- Adjust pin configurations if using different hardware
- Modify the initialization code in `sealobd.ino`

If you're using a different ESP32 board, you'll need to remove the `#include <M5Unified.h>` line from the main sketch.

## Support

If you have issues:
1. Check the serial monitor output for error messages
2. Verify all your settings in `arduino_secrets.h`
3. Make sure all required libraries are installed
4. Try resetting both the ESP32 and OBDLink CX

## Future Improvements

Possible additions to the project:
- Add a display to show data locally
- Support for other BYD models
- Calculate charging efficiency
- Add estimated range calculations
- Web interface for configuration
