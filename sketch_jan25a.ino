#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <M5Unified.h>
#include <Arduino.h>
#include <time.h>
#include "ELMduino.h"
#include "BLEClientSerial.h"
#include "arduino_secrets.h"

BLEClientSerial BLESerial;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

#define DEBUG_PORT Serial
#define ELM_PORT BLESerial

ELM327 myELM327;

typedef enum { 
  OBD_SETUP,
  OBD_READ_SOC,
  WIFI_CONNECT,
  NTP_SYNC,        // New state for NTP synchronization
  MQTT_CONNECT,
  MQTT_PUBLISH,
  WAIT_CYCLE
} main_states;

main_states main_state = OBD_SETUP;
int nb_query_state = SEND_COMMAND;
float state_of_charge = 0.0;
bool obd_connected = false;
bool wifi_connected = false;
bool mqtt_connected = false;
bool time_synced = false;  // Track if time has been synchronized

// Bluetooth timeout tracking variables
int consecutive_bt_timeouts = 0;
const int MAX_CONSECUTIVE_BT_TIMEOUTS = 2;
bool car_connection_lost = false;

// Timeout variables
unsigned long elm_connection_start_time = 0;
unsigned long elm_soc_start_time = 0;
unsigned long ntp_sync_start_time = 0;
const unsigned long ELM_CONNECTION_TIMEOUT = 30000; // 30 seconds for BLE connection
const unsigned long ELM_INIT_TIMEOUT = 15000;       // 15 seconds for ELM initialization
const unsigned long ELM_SOC_TIMEOUT = 10000;        // 10 seconds for SoC reading
const unsigned long NTP_SYNC_TIMEOUT = 10000;       // 10 seconds for NTP sync

// NTP Configuration
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";
const char* time_zone = "AEST-10AEDT,M10.1.0,M4.1.0/3";  // Australia/Sydney timezone

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;
unsigned long lastStatusUpdate = 0;
unsigned long updateInterval = 0;
bool willRetain = true;
int willQos = 1;

const char broker[] = "10.0.0.122";
int port = 1883;
const char topic[] = "bydseal/soc";
const char status_topic[] = "bydseal/status";
const char last_update_topic[] = "bydseal/last_update";

uint8_t ctoi(uint8_t value) {
  if (value >= 'A' && value <= 'F')  // Handle hexadecimal letters
    return value - 'A' + 10;
  else if (value >= '0' && value <= '9')  // Handle digits
    return value - '0';
  else
    return 0;  // Default or invalid input
}

void wifi_setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  wifi_connected = true;
}

bool sync_time_with_ntp() {
  Serial.println("Synchronizing time with NTP servers...");
  
  // Configure NTP with multiple servers for redundancy
  configTime(0, 0, ntpServer1, ntpServer2, ntpServer3);
  
  // Set timezone for Australia/Sydney
  setenv("TZ", time_zone, 1);
  tzset();
  
  // Wait for time synchronization
  time_t now = 0;
  struct tm timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 15;  // Try for up to 15 seconds
  
  while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
    Serial.print("Waiting for system time to be set... (");
    Serial.print(retry);
    Serial.print("/");
    Serial.print(retry_count);
    Serial.println(")");
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }
  
  if (timeinfo.tm_year < (2024 - 1900)) {
    Serial.println("Failed to obtain time from NTP servers");
    return false;
  }
  
  // Print synchronized time
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
  Serial.print("Time synchronized: ");
  Serial.println(time_str);
  
  return true;
}

void mqtt_setup() {
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    mqtt_connected = false;
    return;
  }
  Serial.println("You're connected to the MQTT broker!");
  mqtt_connected = true;
}

void wifi_disconnect() {
  if (wifi_connected) {
    Serial.println("Disconnecting WiFi...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_connected = false;
    Serial.println("WiFi disconnected");
  }
}

void mqtt_disconnect() {
  if (mqtt_connected) {
    Serial.println("Disconnecting MQTT...");
    mqttClient.stop();
    mqtt_connected = false;
    Serial.println("MQTT disconnected");
  }
}

void obd_disconnect() {
  if (obd_connected) {
    Serial.println("Disconnecting OBD/ELM327...");
    // Send reset command and close connection
    myELM327.sendCommand_Blocking("ATZ"); // Reset ELM327
    ELM_PORT.end(); // Close BLE connection
    obd_connected = false;
    Serial.println("OBD/ELM327 disconnected");
  }
}

void publish_status(const char* status_message) {
  if (mqtt_connected) {
    Serial.print("Publishing status: ");
    Serial.println(status_message);
    mqttClient.beginMessage(status_topic, willRetain);
    mqttClient.print(status_message);
    mqttClient.endMessage();
  }
}

void publish_last_update() {
  if (mqtt_connected) {
    if (!time_synced) {
      Serial.println("Warning: Time not synchronized, publishing placeholder timestamp");
      mqttClient.beginMessage(last_update_topic, willRetain);
      mqttClient.print("TIME_NOT_SYNCED");
      mqttClient.endMessage();
      return;
    }
    
    // Get current time components
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format as readable datetime string with timezone
    char datetime_str[64];
    strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    
    Serial.print("Publishing last update time: ");
    Serial.println(datetime_str);
    mqttClient.beginMessage(last_update_topic, willRetain);
    mqttClient.print(datetime_str);
    mqttClient.endMessage();
  }
}

bool is_bluetooth_timeout_error(const char* error_message) {
  // Check if the error is related to Bluetooth connection/timeout
  return (strstr(error_message, "ELM_BLE_CONNECTION_TIMEOUT") != NULL ||
          strstr(error_message, "ELM_INIT_TIMEOUT") != NULL ||
          strstr(error_message, "SOC_READ_TIMEOUT") != NULL);
}

void handle_elm_timeout(const char* error_message) {
  Serial.print("ELM327 timeout: ");
  Serial.println(error_message);
  
  // Track consecutive Bluetooth timeouts
  if (is_bluetooth_timeout_error(error_message)) {
    consecutive_bt_timeouts++;
    Serial.print("Consecutive Bluetooth timeouts: ");
    Serial.println(consecutive_bt_timeouts);
    
    // Check if we've reached the threshold for "No Car Connection"
    if (consecutive_bt_timeouts >= MAX_CONSECUTIVE_BT_TIMEOUTS) {
      car_connection_lost = true;
      Serial.println("Maximum consecutive Bluetooth timeouts reached - No Car Connection");
    }
  } else {
    // Reset counter for non-Bluetooth errors
    consecutive_bt_timeouts = 0;
    car_connection_lost = false;
  }
  
  // Try to establish minimal connections to report failure
  if (!wifi_connected) {
    wifi_setup();
  }
  
  // Sync time if not done yet and WiFi is connected
  if (!time_synced && wifi_connected) {
    time_synced = sync_time_with_ntp();
  }
  
  if (!mqtt_connected && wifi_connected) {
    mqtt_setup();
  }
  
  if (mqtt_connected) {
    // Publish appropriate status based on car connection state
    if (car_connection_lost) {
      publish_status("No Car Connection");
    } else {
      publish_status(error_message);
    }
    publish_last_update();
    mqtt_disconnect();
    wifi_disconnect();
  }
  
  // Clean up any partial connections
  obd_disconnect();
  
  // Wait before next retry cycle
  main_state = WAIT_CYCLE;
  updateInterval = 60000; // Wait 1 minute before retry
}

void reset_bluetooth_timeout_counter() {
  if (consecutive_bt_timeouts > 0) {
    Serial.print("Resetting Bluetooth timeout counter from ");
    Serial.println(consecutive_bt_timeouts);
    consecutive_bt_timeouts = 0;
    car_connection_lost = false;
  }
}

void setup() {
  DEBUG_PORT.begin(115200);
  Serial.println("Starting OBD-first flow with NTP time sync...");
  
  // Initialize but don't connect yet - connection will happen in OBD_SETUP state
}

void status_update() {
  switch (main_state) {
    case OBD_SETUP:
      {
        static enum { 
          SETUP_START, 
          BLE_CONNECTING, 
          ELM_INITIALIZING 
        } setup_substep = SETUP_START;
        
        switch (setup_substep) {
          case SETUP_START:
            Serial.println("Step 1: Setting up OBD connection...");
            ELM_PORT.begin("OBDLink CX");
            elm_connection_start_time = millis();
            setup_substep = BLE_CONNECTING;
            break;
            
          case BLE_CONNECTING:
            Serial.println("Attempting BLE connection with timeout...");
            if (!ELM_PORT.connect(15000)) { // 15 second timeout
              Serial.println("BLE connection failed or timed out");
              handle_elm_timeout("ELM_BLE_CONNECTION_TIMEOUT");
              setup_substep = SETUP_START; // Reset for next cycle
              return;
            }
            Serial.println("BLE connected, initializing ELM327...");
            elm_connection_start_time = millis(); // Reuse for init timeout
            setup_substep = ELM_INITIALIZING;
            break;
            
          case ELM_INITIALIZING:
            if (!myELM327.begin(ELM_PORT, true, 2000)) {
              if (millis() - elm_connection_start_time > ELM_INIT_TIMEOUT) {
                handle_elm_timeout("ELM_INIT_TIMEOUT");
                setup_substep = SETUP_START; // Reset for next cycle
                return;
              }
              Serial.println("Initializing ELM327...");
              delay(1000);
              return; // Stay in this state until initialized or timeout
            }
            
            Serial.println("Connected to ELM327, configuring...");
            
            // Initialize ELM327 with required commands
            myELM327.sendCommand_Blocking("ATZ");
            myELM327.sendCommand_Blocking("ATD");
            myELM327.sendCommand_Blocking("ATD0");
            myELM327.sendCommand_Blocking("ATH1");
            myELM327.sendCommand_Blocking("ATSP6");
            myELM327.sendCommand_Blocking("ATE0");
            myELM327.sendCommand_Blocking("ATH1");
            myELM327.sendCommand_Blocking("ATM0");
            myELM327.sendCommand_Blocking("ATS0");
            myELM327.sendCommand_Blocking("ATAT1");
            myELM327.sendCommand_Blocking("ATAL");
            myELM327.sendCommand_Blocking("STCSEGT1");
            myELM327.sendCommand_Blocking("ATST96");
            myELM327.sendCommand_Blocking("ATSH7E7");
            
            obd_connected = true;
            main_state = OBD_READ_SOC;
            nb_query_state = SEND_COMMAND;
            setup_substep = SETUP_START; // Reset for next cycle
            Serial.println("OBD setup complete, moving to SoC reading...");
            break;
        }
        break;
      }

    case OBD_READ_SOC:
      {
        Serial.println("Step 2: Reading State of Charge from OBD...");
        
        if (nb_query_state == SEND_COMMAND) {
          myELM327.sendCommand("221FFC");
          nb_query_state = WAITING_RESP;
          elm_soc_start_time = millis(); // Start SoC reading timeout
        } else if (nb_query_state == WAITING_RESP) {
          // Check for timeout
          if (millis() - elm_soc_start_time > ELM_SOC_TIMEOUT) {
            Serial.println("SoC reading timeout");
            handle_elm_timeout("SOC_READ_TIMEOUT");
            return;
          }
          
          Serial.print("Awaiting response from ELM327...");
          myELM327.get_response();
        }

        if (myELM327.nb_rx_state == ELM_SUCCESS) {
          int A = (ctoi(myELM327.payload[11]) << 4) | ctoi(myELM327.payload[12]);
          int B = (ctoi(myELM327.payload[13]) << 4) | ctoi(myELM327.payload[14]);
          Serial.print("A: ");
          Serial.println(A);
          Serial.print("B: ");
          Serial.println(B);
          B = B * 256;
          float soc = float(A + B) / 100.0;
          state_of_charge = soc;
          Serial.print("State of Charge: ");
          Serial.println(state_of_charge, 2);
          
          // Reset Bluetooth timeout counter on successful SoC read
          reset_bluetooth_timeout_counter();
          
          nb_query_state = SEND_COMMAND;
          main_state = WIFI_CONNECT;
          Serial.println("SoC read complete, moving to WiFi connection...");
        }
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
          nb_query_state = SEND_COMMAND;
          myELM327.printError();
          
          // Report SoC reading failure
          handle_elm_timeout("SOC_READ_FAILED");
          return;
        }
        break;
      }

    case WIFI_CONNECT:
      {
        Serial.println("Step 3: Connecting to WiFi...");
        wifi_setup();
        main_state = NTP_SYNC;  // Move to NTP sync instead of MQTT
        Serial.println("WiFi connected, moving to NTP time synchronization...");
        break;
      }

    case NTP_SYNC:
      {
        Serial.println("Step 4: Synchronizing time with NTP servers...");
        ntp_sync_start_time = millis();
        
        if (sync_time_with_ntp()) {
          time_synced = true;
          main_state = MQTT_CONNECT;
          Serial.println("Time synchronized, moving to MQTT connection...");
        } else {
          if (millis() - ntp_sync_start_time > NTP_SYNC_TIMEOUT) {
            Serial.println("NTP synchronization timeout, continuing without accurate time...");
            time_synced = false;
            main_state = MQTT_CONNECT;
          } else {
            Serial.println("NTP sync failed, retrying in 2 seconds...");
            delay(2000);
          }
        }
        break;
      }

    case MQTT_CONNECT:
      {
        Serial.println("Step 5: Connecting to MQTT broker...");
        mqtt_setup();
        if (mqtt_connected) {
          main_state = MQTT_PUBLISH;
          Serial.println("MQTT connected, moving to publish SoC...");
        } else {
          Serial.println("MQTT connection failed, retrying in 5 seconds...");
          delay(5000);
        }
        break;
      }

    case MQTT_PUBLISH:
      {
        Serial.println("Step 6: Publishing State of Charge to MQTT...");
        if (mqtt_connected) {
          // Publish appropriate status based on car connection state
          if (car_connection_lost) {
            publish_status("No Car Connection");
          } else {
            publish_status("CONNECTED");
          }
          
          // Only publish SoC data if we have a car connection
          if (!car_connection_lost) {
            mqttClient.beginMessage(topic, willRetain);
            mqttClient.print(state_of_charge);
            mqttClient.endMessage();
            Serial.print("Published SoC: ");
            Serial.print(state_of_charge);
            Serial.print(" to topic: ");
            Serial.println(topic);
          } else {
            Serial.println("Skipping SoC publish due to no car connection");
          }
          
          // Publish last update time (with proper timezone)
          publish_last_update();
          
          // Wait a moment for messages to be sent
          delay(1000);
        }
        
        // Properly close all connections
        mqtt_disconnect();
        wifi_disconnect();
        obd_disconnect();
        
        main_state = WAIT_CYCLE;
        updateInterval = 300000; // Wait 5 minutes before next cycle
        Serial.println("All connections closed. Cycle complete, waiting for next update...");
        break;
      }

    case WAIT_CYCLE:
      {
        // Reset for next cycle - start from OBD setup again
        main_state = OBD_SETUP;
        updateInterval = 0; // Immediate next cycle
        Serial.println("Starting new cycle...");
        break;
      }
  }
}

void loop() {
  // Keep MQTT connection alive only if connected
  if (mqtt_connected && WiFi.status() == WL_CONNECTED) {
    mqttClient.poll();
  }
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusUpdate >= updateInterval) {
    status_update();
    lastStatusUpdate = currentMillis;
  }
}
