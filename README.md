# SealOBD

Reads the current state of charge from a BYD Seal (and possibly others), using an ESP32 conneting to an OBDLink CX using BLE. Publishes the resulting SOC, Battery Voltage and Battery Temp (just testing this value at the moment) to an MQTT server.

Requires the ELMDuino library https://docs.arduino.cc/libraries/elmduino/
