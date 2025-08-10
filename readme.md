# BYD Seal OBD Monitor

**BYD Seal OBD Monitor for M5Stack AtomS3 Lite**

Reads the current state of charge from a BYD Seal (and possibly others), using an M5Stack AtomS3 Lite ESP32 connecting to an OBDLink CX using Bluetooth Low Energy (BLE).

Requires the ELMDuino library https://docs.arduino.cc/libraries/elmduino/

The code contains a slightly tweaked BLE Serial library from https://github.com/vdvornichenko/obd-ble-serial

## What It Does

This project turns an M5Stack AtomS3 Lite into a car battery monitor for your BYD Seal. It connects to your car through an OBDLink CX adapter using Bluetooth, reads important battery information, and sends it to your home automation system using MQTT. The built-in RGB LED provides visual status indication throughout the process.

### Information It Collects
- **Battery State of Charge** - How full your battery is (as a percentage)
- **Battery Temperature** - How hot or cold your battery pack is
- **Battery Voltage** - The current voltage of your battery
- **Total Charges** - How many times the car has been charged
- **Total kWh Charged** - Total energy put into the battery over its lifetime
- **Total kWh Discharged** - Total energy taken out of the battery over its lifetime

### Visual Status Indication
The built-in RGB LED shows what the monitor is doing:
- 🟣 **Purple** - Setting up and connecting to car
- 🟢 **Green** - Reading data from car's OBD system
- 🔵 **Blue** - Network operations (WiFi, MQTT, time sync)
- 🟡 **Yellow** - Waiting for next update cycle (5 minutes)
- 🔴 **Red** - Error occurred (will retry after 1 minute)
- **Green Blinks** - Data successfully published to MQTT

## What You Need

### Hardware
- **M5Stack AtomS3 Lite** - This project is specifically designed for this ESP32-S3 based board with built-in RGB LED
- **OBDLink CX** - A Bluetooth OBD adapter that plugs into your car
- **BYD Seal** or similar BYD electric vehicle

### Software Libraries
- Arduino IDE
- ELMDuino library
- ArduinoMqttClient library
- M5AtomS3 library (for LED control and hardware functions)

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
2. Install the required libraries through the Library Manager:
   - ELMDuino
   - ArduinoMqttClient
   - M5AtomS3
3. Copy all the project files to your Arduino project folder
4. Upload the code to your M5Stack AtomS3 Lite

### 3. Connect to Your Car
1. Plug the OBDLink CX into your car's OBD port (usually under the dashboard)
2. Power on the M5Stack AtomS3 Lite
3. Watch the LED status - it will show purple while connecting
4. The device will automatically find and connect to the OBDLink CX

## How It Works

The monitor follows these steps every 5 minutes:

1. **Connect to Car** (🟣 Purple LED) - Establishes Bluetooth connection to OBDLink CX
2. **Read Battery Data** (🟢 Green LED) - Gets all the battery information from your car
3. **Connect to WiFi** (🔵 Blue LED) - Joins your home network
4. **Sync Time** (🔵 Blue LED) - Gets the current time from internet time servers
5. **Send Data** (🔵 Blue LED) - Publishes all the information to your MQTT broker
6. **Success** (Green blinks) - Confirms data was sent successfully
7. **Sleep** (🟡 Yellow LED) - Waits 5 minutes before doing it all again

If something goes wrong, the LED turns red and the system will retry after 1 minute instead of 5.

## Understanding the LED Status

The RGB LED provides real-time feedback about what the monitor is doing:

| LED Color | Status | Description |
|-----------|--------|-------------|
| 🟣 Purple | Setup/Connecting | Connecting to OBDLink CX or initializing |
| 🟢 Green | Reading OBD Data | Successfully reading battery information |
| 🔵 Blue | Network Operations | WiFi connection, MQTT publishing, time sync |
| 🟡 Yellow | Waiting | Normal wait period between updates (5 minutes) |
| 🔴 Red | Error | Something went wrong, will retry in 1 minute |
| 🟢 Blinks | Success | Data successfully published to MQTT |

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
- **LEDManager** - Controls the RGB LED status indication
- **Logger** - Shows what's happening (for debugging)
- **BLEClientSerial** - Bluetooth communication with OBDLink

## Troubleshooting

### LED Status Troubleshooting

**LED stays purple for a long time**
- OBDLink CX might not be plugged in properly
- Car might not be turned on
- Bluetooth pairing issues

**LED turns red immediately**
- Check serial monitor for specific error messages
- Verify OBDLink CX is working

**LED stays blue**
- Network connection issues
- Check WiFi credentials
- MQTT broker might be unreachable

**LED stays yellow**
- Normal operation - waiting for next update cycle

### Common Issues

**Can't connect to OBDLink CX**
- Make sure the OBDLink CX is plugged into your car
- Check that your car is turned on
- The adapter name should be "OBDLink CX"
- LED will be purple during connection attempts

**MQTT not working**
- Verify your MQTT broker IP address is correct
- Check username and password
- Make sure MQTT broker is running
- LED will show blue during network operations

**WiFi won't connect**
- Double-check your WiFi name and password
- Make sure the M5Stack is in range of your router
- LED will stay blue if WiFi connection fails

**Device doesn't work without serial monitor**
- This has been fixed - the device now works independently
- LED status provides feedback without needing a computer connection

## Customization

### Change Update Frequency
In `Config.h`, modify these values (in milliseconds):
- `NORMAL_UPDATE = 300000` - Normal update every 5 minutes
- `ERROR_RETRY = 60000` - Retry after error every 1 minute

### Change MQTT Topics
Edit the topic names in `Config.h` under the MQTT namespace.

### Adjust LED Brightness
In `Config.h`, modify:
- `LED_BRIGHTNESS = 50` - Brightness from 0-100%

### Disable LED (if needed)
In `Config.h`, set:
- `LED_ENABLED = false` - Turns off all LED functionality

### Adjust Timeouts
If connections are timing out, increase the timeout values in `Config.h`.

## Safety Notes

- The monitor only reads data from your car - it doesn't change anything
- Always plug the OBDLink CX in carefully to avoid damaging the port
- The system is designed to handle connection losses gracefully
- The M5Stack AtomS3 Lite is designed for continuous operation

## How to Use the Data

The MQTT data can be used with:
- **Home Assistant** - Create sensors and automations
- **Node-RED** - Build flows and dashboards
- **Grafana** - Create beautiful graphs of your battery usage
- **Any MQTT client** - Subscribe to topics and use the data

Example Home Assistant configuration:
```yaml
sensor:
  - platform: mqtt
    name: "BYD Seal Battery SOC"
    state_topic: "bydseal/soc"
    unit_of_measurement: "%"
    device_class: battery
```

## Hardware Specific Information

This project is **specifically designed for the M5Stack AtomS3 Lite**:

### Why M5Stack AtomS3 Lite?
- **Compact Size** - Perfect for permanent car installation
- **Built-in RGB LED** - Provides clear status indication
- **ESP32-S3 Processor** - Powerful and reliable
- **USB-C Power** - Easy to power from car's USB port
- **Robust Design** - Built for embedded applications

### Key Features Used:
- **RGB LED** for status indication
- **Bluetooth LE** for OBDLink CX communication
- **WiFi** for MQTT connectivity
- **Non-blocking operation** - works without serial monitor

### Physical Installation:
- Small enough to hide in your car's cabin
- Can be powered from car's USB port
- LED visible for status monitoring
- No display needed - all feedback via LED and MQTT

## Power Consumption

The M5Stack AtomS3 Lite is designed for low power operation:
- **Active** - ~100mA during data reading and transmission
- **Waiting** - ~20mA during 5-minute wait periods
- **Sleep potential** - Future versions could add deep sleep for even lower power

## Support

If you have issues:
1. **Check the LED status** - it will tell you what's happening
2. Connect to serial monitor for detailed error messages
3. Verify all your settings in `arduino_secrets.h`
4. Make sure all required libraries are installed
5. Try resetting both the M5Stack and OBDLink CX

## Future Improvements

Possible additions to the project:
- **Deep sleep mode** - Reduce power consumption further
- **Display integration** - Add external display for local data viewing
- **Web interface** - Configure settings via web browser
- **OTA updates** - Update firmware wirelessly
- **Multiple car support** - Support different BYD models
- **Data logging** - Store data locally on SD card
- **Push notifications** - Alert when battery is low or fully charged
- **Charging analysis** - Calculate charging efficiency and patterns

## Version History

- **v2.0** - Complete rewrite for M5Stack AtomS3 Lite with LED status indication
- **v1.0** - Initial version for generic ESP32 boards

## License

This project is open source. Feel free to modify and improve it for your needs.
