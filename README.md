
GK WiFi MQTT+LiFePO4wered software
===

This Arduino-style firmware for the DIYGeiger GK WiFi kit (attached to the GK Plus kit) takes some inspiration from the provided GK WiFi base software, Azure IoT Hub's sample code, LiFePO4wered docs and samples, and my own past work on AirQualSniff to provide:

* Status information via an I2C-attached OLED screen
    * Most recent reading
    * Current time
    * Boot, network connection, and telemetry status
    * Battery status
* GK Plus data ingestion via hardware UART
* Current time NMEA sentences out over software UART (the GK Plus can accept GPS NMEA sentences)
* LiFePO4wered battery management system configuration, control, and status
* Network-based logging of data
    * mDNS client (finding telemetry server)
    * NTP client (setting GK Plus time)
    * MQTT client (sending data)
* Data reported
    * Geiger counter readings
    * Battery status
    * Uptime

The front panel of my Geiger counter does not expose any inputs on the GK WiFi unless you count the power button connected to the LiFePO4wered BMS, so it aims to be fully self-contained.

Dependency LKG versions
===

* Arduino IDE: LKG is 1.18.13 (but newer will probably be fine)
* OLED library: https://github.com/ThingPulse/esp8266-oled-ssd1306.git c1fa10ea5e3700cfde1d032fa20b468bc43c997c
    * "ESP8266 and ESP32 Oled Driver for SSD1306 display" in Library Manager
    * Must be newer than 4.1.0

Board configuration
===

The GK WiFi kit is an ESP8266 on a carrier board soldered to an application-specific carrier board.

Board Manager URI: http://arduino.esp8266.com/stable/package_esp8266com_index.json

* "Tools" menu in Arduino IDE
* Board: "Generic ESP8266 Module"
* Flash: GK-WiFi build instructions say "4M (1M SPIFFS)" and I think that's equivalent to "4MB (FS:1MB OTA:~1019KB)"
* Upload speed: my equipment seems to tolerate 256000 baud
