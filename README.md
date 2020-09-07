
GK WiFi Azure+LiFePO4wered software
===

Right now this has chunks from the Azure sample and the GK WiFi software. I'll need to make sure licensing is in the clear before I publish it.

Dependency LKG versions
===

Arduino IDE: LKG is 1.18.13
OLED library: https://github.com/ThingPulse/esp8266-oled-ssd1306.git c1fa10ea5e3700cfde1d032fa20b468bc43c997c
Azure libs:
  https://github.com/Azure/azure-iot-arduino
  AzureIoTHub
  AzureIoTUtility
  AzureIoTProtocol_MQTT
  AzureIoTProtocol_HTTP
  all ^^^ 1.3.9
ESP BSP: 2.7.4 tweaked per azure-iot-arduino instructions
  Note that Windows Store puts the ESP8266 BSP at
  C:\Users\Jon\Documents\ArduinoData\packages\esp8266\hardware\esp8266\2.7.4\cores\esp8266\Arduino.h
  Note that you don't want platform.local.txt from the Azure ESP8266 sample because it's full of old cruft. All you want is
  `build.extra_flags=-DDONT_USE_UPLOADTOBLOB -DUSE_BALTIMORE_CERT -DESP8266`

Board configuration
===

"Tools" menu in Arduino IDE
Board: "Generic ESP8266 Module"
Flash: GK-WiFi build instructions say "4M (1M SPIFFS)" and I think that's equivalent to "4MB (FS:1MB OTA:~1019KB)"
Upload speed: my equipment seems to tolerate 256000 baud