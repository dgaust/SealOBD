#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <M5Unified.h>
#include <Arduino.h>
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
    unsigned long currentTime = millis();
    Serial.print("Publishing last update time: ");
    Serial.println(currentTime);
    mqttClient.beginMessage(last_update_topic, willRetain);
    mqttClient.print(currentTime);
    mqttClient.endMessage();
  }
}

void setup() {
  DEBUG_PORT.begin(115200);
  Serial.println("Starting OBD-first flow...");
  
  // Initialize but don't connect yet - connection will happen in OBD_SETUP state
}

void status_update() {
  switch (main_state) {
    case OBD_SETUP:
      {
        Serial.println("Step 1: Setting up OBD connection...");
        
        // Start fresh BLE connection
        ELM_PORT.begin("OBDLink CX");
        
        if (!ELM_PORT.connect()) {
          DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
          // Try to establish minimal connections to report failure
          if (!wifi_connected) {
            wifi_setup();
          }
          if (!mqtt_connected && wifi_connected) {
            mqtt_setup();
          }
          if (mqtt_connected) {
            publish_status("ELM_CONNECTION_FAILED_PHASE1");
            publish_last_update();
            mqtt_disconnect();
            wifi_disconnect();
          }
          delay(5000); // Wait before retry
          return;
        }

        if (!myELM327.begin(ELM_PORT, true, 2000)) {
          Serial.println("Couldn't connect to OBD scanner - Phase 2");
          // Try to establish minimal connections to report failure
          if (!wifi_connected) {
            wifi_setup();
          }
          if (!mqtt_connected && wifi_connected) {
            mqtt_setup();
          }
          if (mqtt_connected) {
            publish_status("ELM_CONNECTION_FAILED_PHASE2");
            publish_last_update();
            mqtt_disconnect();
            wifi_disconnect();
          }
          delay(5000); // Wait before retry
          return;
        }

        Serial.println("Connected to ELM327, initializing...");
        
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
        Serial.println("OBD setup complete, moving to SoC reading...");
        break;
      }

    case OBD_READ_SOC:
      {
        Serial.println("Step 2: Reading State of Charge from OBD...");
        
        if (nb_query_state == SEND_COMMAND) {
          myELM327.sendCommand("221FFC");
          nb_query_state = WAITING_RESP;
        } else if (nb_query_state == WAITING_RESP) {
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
          
          nb_query_state = SEND_COMMAND;
          main_state = WIFI_CONNECT;
          Serial.println("SoC read complete, moving to WiFi connection...");
        }
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
          nb_query_state = SEND_COMMAND;
          myELM327.printError();
          
          // Report SoC reading failure
          if (!wifi_connected) {
            wifi_setup();
          }
          if (!mqtt_connected && wifi_connected) {
            mqtt_setup();
          }
          if (mqtt_connected) {
            publish_status("SOC_READ_FAILED");
            publish_last_update();
            mqtt_disconnect();
            wifi_disconnect();
          }
          
          delay(2000);
        }
        break;
      }

    case WIFI_CONNECT:
      {
        Serial.println("Step 3: Connecting to WiFi...");
        wifi_setup();
        main_state = MQTT_CONNECT;
        Serial.println("WiFi connected, moving to MQTT connection...");
        break;
      }

    case MQTT_CONNECT:
      {
        Serial.println("Step 4: Connecting to MQTT broker...");
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
        Serial.println("Step 5: Publishing State of Charge to MQTT...");
        if (mqtt_connected) {
          // Publish successful status
          publish_status("CONNECTED");
          
          // Publish SoC data
          mqttClient.beginMessage(topic, willRetain);
          mqttClient.print(state_of_charge);
          mqttClient.endMessage();
          Serial.print("Published SoC: ");
          Serial.print(state_of_charge);
          Serial.print(" to topic: ");
          Serial.println(topic);
          
          // Publish last update time
          publish_last_update();
          
          // Wait a moment for messages to be sent
          delay(1000);
        }
        
        // Properly close all connections
        mqtt_disconnect();
        wifi_disconnect();
        obd_disconnect();
        
        main_state = WAIT_CYCLE;
        updateInterval = 300000; // Wait 2 minutes before next cycle
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