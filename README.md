
GK WiFi Azure+LiFePO4wered software
===

As described in LICENSE.md, this Arduino program for the DIYGeiger GK WiFi and GK Plus kits takes some inspiration from the provided DIYGeiger software and the Azure IoT Hub sample to push data from the GK Plus to an Azure IoT Hub. It also governs power for the system through the LiFePO4wered/Pi+ battery management system.

Dependency LKG versions
===

* Arduino IDE: LKG is 1.18.13
* OLED library: https://github.com/ThingPulse/esp8266-oled-ssd1306.git c1fa10ea5e3700cfde1d032fa20b468bc43c997c
* Azure libs:
  * https://github.com/Azure/azure-iot-arduino
  * AzureIoTHub
  * AzureIoTUtility
  * AzureIoTProtocol_MQTT
  * AzureIoTProtocol_HTTP
  * all ^^^ 1.3.9
* ESP BSP: 2.7.4 tweaked per azure-iot-arduino instructions
  * Note that Windows Store puts the ESP8266 BSP at
  * %HomePath%\Documents\ArduinoData\packages\esp8266\hardware\esp8266\2.7.4\cores\esp8266\Arduino.h
  * Note that you don't want platform.local.txt from the Azure ESP8266 sample because it's full of old cruft. All you want is
  * `build.extra_flags=-DDONT_USE_UPLOADTOBLOB -DUSE_BALTIMORE_CERT -DESP8266`

Board configuration
===

* "Tools" menu in Arduino IDE
* Board: "Generic ESP8266 Module"
* Flash: GK-WiFi build instructions say "4M (1M SPIFFS)" and I think that's equivalent to "4MB (FS:1MB OTA:~1019KB)"
* Upload speed: my equipment seems to tolerate 256000 baud
