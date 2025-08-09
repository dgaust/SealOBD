#ifndef CONFIG_H
#define CONFIG_H

#include "arduino_secrets.h"

// Debug Configuration
#define DEBUG_ENABLED true
#define DEBUG_PORT Serial
#define DEBUG_BAUD_RATE 115200

// Connection Timeouts (ms)
namespace Timeouts {
    constexpr unsigned long ELM_CONNECTION = 30000;
    constexpr unsigned long ELM_INIT = 15000;
    constexpr unsigned long ELM_COMMAND = 10000;
    constexpr unsigned long NTP_SYNC = 10000;
    constexpr unsigned long BLE_CONNECTION = 15000;
}

// Update Intervals (ms)
namespace Intervals {
    constexpr unsigned long NORMAL_UPDATE = 300000;  // 5 minutes
    constexpr unsigned long ERROR_RETRY = 60000;     // 1 minute
    constexpr unsigned long INITIAL_DELAY = 0;
}

// MQTT Configuration
namespace MQTT {
    const char* const BROKER = SECRET_MQTT_IP;
    const int PORT = SECRET_MQTT_PORT;
    const char* const USER = SECRET_MQTT_USER;
    const char* const PASS = SECRET_MQTT_PASS;
    
    // Topics
    const char* const TOPIC_SOC = "bydseal/soc";
    const char* const TOPIC_TEMP = "bydseal/battery_temp";
    const char* const TOPIC_VOLTAGE = "bydseal/battery_voltage";
    const char* const TOPIC_STATUS = "bydseal/status";
    const char* const TOPIC_LAST_UPDATE = "bydseal/last_update";
    const char* const TOPIC_CHARGES_UPDATE = "bydseal/total_charges";
    const char* const TOPIC_KWH_CHARGED_UPDATE = "bydseal/kwh_charged";
    const char* const TOPIC_KWH_DISCHARGED_UPDATE = "bydseal/kwh_discharged";
    
    const bool RETAIN = true;
    const int QOS = 1;
}

// WiFi Configuration
namespace WiFi_Config {
    const char* const SSID = SECRET_SSID;
    const char* const PASSWORD = SECRET_PASS;
}

// NTP Configuration
namespace NTP {
    const char* const SERVER1 = "pool.ntp.org";
    const char* const SERVER2 = "time.nist.gov";
    const char* const SERVER3 = "time.google.com";
    const char* const TIMEZONE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    const int MAX_RETRY = 15;
}

// OBD Configuration
namespace OBD {
    const char* const DEVICE_NAME = "OBDLink CX";
    const int MAX_BT_TIMEOUTS = 2;
    
    // OBD Commands
    const char* const CMD_SOC = "221FFC";
    const char* const CMD_TEMP = "220032";
    const char* const CMD_VOLTAGE = "220008";
    const char* const CMD_TOTALCHARGES = "22000B"; // A + B*256 - 22000B
    const char* const CMD_TOTALKWHCHARGE = "220011";
    const char* const CMD_TOTALKWHDISCHARGE = "220012";

    // ELM327 Initialization Commands
    inline const char* INIT_COMMANDS[] = {
        "ATZ",      // Reset
        "ATD",      // Set defaults
        "ATD0",     // Set defaults (no echo)
        "ATH1",     // Headers on
        "ATSP6",    // Set protocol to ISO 15765-4 CAN (11 bit ID, 500 kbaud)
        "ATE0",     // Echo off
        "ATM0",     // Memory off
        "ATS0",     // Spaces off
        "ATAT1",    // Adaptive timing on
        "ATAL",     // Allow long messages
        "STCSEGT1", // Custom timing
        "ATST96",   // Set timeout
        "ATSH7E7"   // Set header
    };
    const int INIT_COMMANDS_COUNT = 13;
}

// Application States
enum class AppState {
    OBD_SETUP,
    OBD_READ_SOC,
    OBD_READ_BATTERY_TEMP,
    OBD_READ_BATTERY_VOLTAGE,
    OBD_CHARGE_TIMES,
    OBD_TOTAL_CHARGED_KWH,
    OBB_TOTAL_DISCHARGED_KWH,
    WIFI_CONNECT,
    NTP_SYNC,
    MQTT_CONNECT,
    MQTT_PUBLISH,
    WAIT_CYCLE
};

// Error Messages
namespace ErrorMessages {
    const char* const BLE_TIMEOUT = "ELM_BLE_CONNECTION_TIMEOUT";
    const char* const INIT_TIMEOUT = "ELM_INIT_TIMEOUT";
    const char* const SOC_TIMEOUT = "SOC_READ_TIMEOUT";
    const char* const TEMP_TIMEOUT = "TEMP_READ_TIMEOUT";
    const char* const VOLTAGE_TIMEOUT = "VOLTAGE_READ_TIMEOUT";
    const char* const SOC_FAILED = "SOC_READ_FAILED";
    const char* const TEMP_FAILED = "TEMP_READ_FAILED";
    const char* const VOLTAGE_FAILED = "VOLTAGE_READ_FAILED";
    const char* const TIMES_CHARGED_FAILED = "TIMES_CHARGED_READ_FAILED";
    const char* const TOTAL_KWH_CHARGED_FAILED = "TOTAL_KWH_CHARGED_FAILED";
    const char* const TOTAL_KWH_DISCHARGED_FAILED = "TOTAL_KWH_DISCHARGED_FAILED";
    const char* const NO_CAR = "No Car Connection";
    const char* const CONNECTED = "CONNECTED";
    const char* const TIME_NOT_SYNCED = "TIME_NOT_SYNCED";
}

#endif // CONFIG_H