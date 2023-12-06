
// MyGKWiFi: a custom GK WiFi firmware
// License: see LICENSE.md
// What does it do: see README.md

//
// Config and Includes
//

//#define MYGKWIFI_DEBUG

// Unless and until there are some major RAM savings somewhere, this is being cut.
// I get x509 init OOM errors and crashes on second samples if local var dynamic memory is below about 45700 bytes.
//#define SEND_TIME_VIA_FAUX_GPS

#include "gkplus_datapoint.h"
#include "Credentials.h"
#include "BMS.h"
#include "Log.h"
#ifdef SEND_TIME_VIA_FAUX_GPS
#include <SoftwareSerial.h>
#endif // SEND_TIME_VIA_FAUX_GPS
#include "LinedDisplay.h"

// NTP synchronization is handled asynchronously, so the Azure SDK just checks whether
// or not the time has been updated to something relatively recent to tell if the sync
// has worked.
// Times before ~2010 (1970 + 40 years) are invalid
#include <time.h>
#define MIN_EPOCH (40 * 365 * 24 * 3600)

// Work in GK-WiFi_v2_0 includes

#include <ESP8266WiFi.h>
#include <Wire.h>                       // for I2C

//----------------------------------------------------------------------------------------------+
//                               PIN MAP for ESP8266
//----------------------------------------------------------------------------------------------+
#define SDA_PIN        0                // I2C SDA pin (pulled up) + PGM button and LED
// TX                  1                // wired to serial comm
#define SCL_PIN        2                // I2C SCL pin (pulled up)
// RX                  3                // wired to serial comm
#define PI_GPIO_14     4                // line to BMS for notifying that shutdown is complete
#define FAUX_GPS_PIN   5                // NMEA time sentences are sent on this pin
#define YEL_LED        12               // off = empty buffer (no GK connection yet)
#define RED_LED        13               // on = setup() running
#define GRN_LED        14               // WiFi connection established
// RUN / PGM           15               // pulled low on carrier board
// WAKE                16               // wired to RESET through cap to wake from sleep

//
// Globals
//

LinedDisplay display(SDA_PIN, SCL_PIN);

// Time globals
// Pacific Standard Time (Seattle)
#ifdef SEND_TIME_VIA_FAUX_GPS
// identifier PST is UTC offset +8
// DST identifier PDT starts in March during week 2, day 0, at 2am November during week 1, day 0 at 1am
// This string format is a POSIX standard; see `man tzset` for details.
const char *tzValue = "PST+8PDT,M3.2.0/2,M11.1.0/2";
SoftwareSerial GpsSerial;
#endif // SEND_TIME_VIA_FAUX_GPS

//
// Setup
//

void setup() {
  // GK-WiFi_v2_0 setup

  Serial.begin(9600);                   // connection to kit requires 9600 baud

  pinMode(RED_LED, OUTPUT);             // setup LED pins
  pinMode(YEL_LED, OUTPUT);
  pinMode(GRN_LED, OUTPUT);
  delay(10); // maybe wait for mux to quiesce? ugly
  digitalWrite(GRN_LED, LOW);
  digitalWrite(YEL_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  Wire.pins(SDA_PIN, SCL_PIN);          // define pins used for I2C
  Wire.begin();

  bms_init(PI_GPIO_14);

  // initialize and setup dispaly . . .
  display.begin();
  // fixed in other situations, but not this one
  display.splash();
#ifdef MYGKWIFI_DEBUG
  display.testPattern();
#endif // MYGKWIFI_DEBUG

  WiFi.mode(WIFI_OFF);

#ifdef SEND_TIME_VIA_FAUX_GPS
  // Set up time
  setenv("TZ", tzValue, 1);
  tzset();
  GpsSerial.begin(9600, SWSERIAL_8N1, -1, FAUX_GPS_PIN);
#endif // SEND_TIME_VIA_FAUX_GPS

  bms_notify_boot_complete();

  digitalWrite(RED_LED, LOW);
}

//
// Loop
//

void loop() {
  static String serialBuffer("");
  static boolean serialLineComplete = false;
  static int loopCount = 0;
  static bool shutdownInitiated = false;
  String serialLine("");
  String phaseString, messageString, dataString;

  //
  // Poll for and handle power loss shutdown notification
  //

  if (shutdownInitiated) {
    dataString = F("Waiting for BMS to cut power");
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
    delay(10000);
    return;
  }

  loopCount += 1;
  // Don't spam the I2C line
  if (loopCount > 1000) {
    loopCount = 0;
    // Check for power shutoff notification
    if (bms_is_shutdown_requested()) {
      shutdownInitiated = true;
      phaseString = F("Shutting down");
      display.writeLine(LinedDisplay::PHASE_LINE, phaseString);
      // TODO: say if low battery or button press
      messageString = F("Shutdown requested by BMS");
      display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
      dataString = F("");
      display.writeLine(LinedDisplay::DATA_LINE, dataString);
      bms_notify_shutdown_complete();
      return;
    }
  }

  //
  // Poll for serial input
  //

  while (Serial.available()) {
    digitalWrite(YEL_LED, HIGH);
    char inChar = (char)(Serial.read());
    if (inChar == '\n' || inChar == '\r') {
      serialLineComplete = true;
      serialLine = serialBuffer;
      serialBuffer = "";
      Log("Got line: '");
      Log(serialLine);
      LogLn("'");
      break;
    } else {
      serialBuffer += inChar;
    }
    // reset screen state
    phaseString = F("Serial Rx");
    display.writeLine(LinedDisplay::PHASE_LINE, phaseString);
    messageString = F("Waiting...");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    display.writeLine(LinedDisplay::DATA_LINE, serialBuffer);
  }

  if (serialLineComplete) {
    digitalWrite(YEL_LED, LOW);
    serialLineComplete = false;
  } else {
#ifdef MYGKWIFI_DEBUG
    delay(1000);
    digitalWrite(YEL_LED, LOW);
    serialLine = gkplus_datapoint::getFakeLine();
#else // MYGKWIFI_DEBUG
    return;
#endif // MYGKWIFI_DEBUG
  }
  serialLine.trim();

  // If for some reason we get empty lines, ignore them.
  // One case is \r\n being parsed as two line endings.
  if (serialLine.length() == 0) {
    LogLn(F("Got empty line from serial"));
    return;
  }

  //
  // Parse serial data into useful data structure
  //

  gkplus_datapoint dataPoint(serialLine);
  boolean dataPointValid = dataPoint.initialized;
  Log(F("dataPoint valid? "));
  LogLn(dataPointValid);

  // write state to display
  if (dataPointValid) {
    messageString = F("Done.");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    dataString = dataPoint.toString();
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
  } else {
    messageString = F("Got bad data");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    dataString = F("");
    display.writeLine(LinedDisplay::DATA_LINE, serialLine);
    return;
  }

  //
  // Start/connect WiFi
  //

  phaseString = F("WiFi bring-up");
  display.writeLine(LinedDisplay::PHASE_LINE, phaseString);
  LogLn(phaseString);
  messageString = F("Starting...");
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  const int WIFI_WAIT_COUNT = 20;
  const int WIFI_WAIT_PERIOD = 500; // ms
  int waitIdx = 0;
  while (WiFi.status() != WL_CONNECTED && waitIdx < WIFI_WAIT_COUNT) {
    dataString = F("WiFi wait count: ");
    waitIdx += 1;
    dataString += String(waitIdx/2);
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
    delay(WIFI_WAIT_PERIOD);
  }
  if (WiFi.status() != WL_CONNECTED) {
    dataString = F("WiFi conn. failed: ");
    dataString += WiFi.status();
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
    Log(dataString);
    WiFi.mode(WIFI_OFF);
    return;
  }
  messageString = F("WiFi connected.");
  LogLn(messageString);
  digitalWrite(GRN_LED, HIGH);
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);

  //
  // Synchronize time via NTP for TLS and IoT Hub interop
  //

  // Time needs to be approximately correct for the sample to work.
  // This is copy-pasted from sample_init.cpp in the Azure SDK and tweaked a little.
  phaseString = F("Syncing Time");
  LogLn(phaseString);
  display.writeLine(LinedDisplay::PHASE_LINE, phaseString);
  messageString = F("Waiting for NTP sync...");
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
  dataString = F("");
  display.writeLine(LinedDisplay::DATA_LINE, dataString);
  {
   time_t epochTime;

   configTime(0, 0, "pool.ntp.org", "time.nist.gov");
   delay(2000);

   while (true) {
       epochTime = time(NULL);

       if (epochTime < MIN_EPOCH) {
           LogLn(F("Fetching NTP epoch time failed! Waiting 2 seconds to retry."));
           delay(2000);
       } else {
           Log(F("Fetched NTP epoch time is: "));
           LogLn(epochTime);
           break;
       }
   }
  }

#ifdef SEND_TIME_VIA_FAUX_GPS
  //
  // Share the current NTP time with the GK-Plus by pretending to be a GPS.
  //
  // TODO: display.writeLine

  // Since I don't have $4000 to spend on a copy of the spec, I'll just go with outdated,
  // potentially incorrect, entirely not-spec-based info from
  // https://www.gpsinformation.org/dale/nmea.htm
  time_t now = time(NULL);
  struct tm utcNow;
  struct tm localNow;
  localtime_r(&now, &localNow);
  gmtime_r(&now, &utcNow);

  String nmeaSentence;
  nmeaSentence += F("$GPZDA,");
  char timeBuf[83]; // NMEA max is 82 chars incl. start and checksum markers
  timeBuf[83-1] = '\0x00'; // paranoid enforced null termination
  strftime(timeBuf, 82, "%H:%M:%S,%d,%m,%Y,", &utcNow);
  nmeaSentence += timeBuf;
  int8_t timezoneHourOffset = -8;
  int8_t timezoneMinuteOffset = 0;
  if (localNow.tm_isdst != 0) {
    timezoneHourOffset += 1;
  }
  nmeaSentence += timezoneHourOffset;
  nmeaSentence += F(",");
  nmeaSentence += timezoneMinuteOffset;
  uint8_t checksum = 0;
  for (int idx = 1; idx < nmeaSentence.length(); idx++) {
    checksum ^= nmeaSentence[idx];
  }
  nmeaSentence += F("*");
  nmeaSentence += String(checksum, HEX);
  nmeaSentence += F("\r\n");
  Serial.print(nmeaSentence);
  GpsSerial.print(nmeaSentence);
#endif // SEND_TIME_VIA_FAUX_GPS

  //
  // Build up and send an IoT Hub message.
  //

  // Construct message, connect to IoT Hub, send message
  phaseString = F("Azure IoT Hub");
  LogLn(phaseString);
  display.writeLine(LinedDisplay::PHASE_LINE, phaseString);
  messageString = F("Creating client");
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
  dataString = F("");
  display.writeLine(LinedDisplay::DATA_LINE, dataString);

  //
  // Celebrate a job well done.
  //

  // Cleanup
  WiFi.mode(WIFI_OFF);
  LogLn(F("End of complete loop()"));
  digitalWrite(GRN_LED, LOW);
}
