
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

#include <AzureIoTHub.h>
#include "AzureIoTProtocol_HTTP.h"
#include "iothubtransporthttp.h"

// sample_init bits from Azure SDK
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

LinedDisplay display(SDA_PIN, SCL_PIN);

/* -- send_confirm_callback --
 * Callback method which executes upon confirmation that a message originating from this device has been received by the IoT Hub in the cloud.
 */
typedef struct _iothub_callback_status_block {
  boolean complete;
  IOTHUB_CLIENT_CONFIRMATION_RESULT result;
} iothub_callback_status_block;
static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
  iothub_callback_status_block *sb = (iothub_callback_status_block*)userContextCallback;
  sb->result = result;
  sb->complete = true;
}

//typedef struct _iothub_conn_callback_status_block {
//  boolean complete;
//  IOTHUB_CLIENT_CONNECTION_STATUS status;
//  IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason;
//} iothub_conn_callback_status_block;
///* -- connection_status_callback --
// * Callback method which executes on receipt of a connection status message from the IoT Hub in the cloud.
// */
//static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
//{
//  iothub_conn_callback_status_block *sb = (iothub_conn_callback_status_block*)user_context;
//  sb->status = result;
//  sb->reason = reason;
//  // This sample DOES NOT take into consideration network outages.
//  if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
//  {
//    Log("The device client is connected to iothub\r\n");
//  }
//  else
//  {
//    Log("The device client has been disconnected\r\n");
//  }
//  sb->complete = true;
//}

// Time globals
// Pacific Standard Time (Seattle)
#ifdef SEND_TIME_VIA_FAUX_GPS
const char *tzValue = "PST+8PDT,M3.2.0/2,M11.1.0/2";
SoftwareSerial GpsSerial;
#endif // SEND_TIME_VIA_FAUX_GPS

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
  IoTHub_Init();

#ifdef SEND_TIME_VIA_FAUX_GPS
  // Set up time
  setenv("TZ", tzValue, 1);
  tzset();
  GpsSerial.begin(9600, SWSERIAL_8N1, -1, FAUX_GPS_PIN);
#endif // SEND_TIME_VIA_FAUX_GPS

  bms_notify_boot_complete();

  digitalWrite(RED_LED, LOW);
}

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

  IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;
  const IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = HTTP_Protocol; // HTTP_Protocol, MQTT_Protocol
  device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(AIH_CONN_STRING, protocol);

  if (device_ll_handle == NULL) {
    messageString = F("Create failed.");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    WiFi.mode(WIFI_OFF);
    digitalWrite(GRN_LED, LOW);
    return;
  }

  LogLn(F("Starting IoT Hub transaction"));
  // Setting the Trusted Certificate.
  IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

  // Example sdk status tracing for troubleshooting
  // This is MQTT-only per https://docs.microsoft.com/en-us/azure/iot-hub/iot-c-sdk-ref/iothub-device-client-ll-h/iothubdeviceclient-ll-setoption
  //bool traceOn = true;
  //IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &traceOn);

  // Setting connection status callback to get indication of connection to iothub
  // Without multithreading and an easy-to-write state machine, monitoring the connection
  // status is not easy enough to do here. Instead only rely on it being stable for one
  // message.
  //iothub_conn_callback_status_block connectionStatusBlock;
  //connectionStatusBlock.complete = false;
  //IOTHUB_CLIENT_RESULT clientSetCbRet = IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, connection_status_callback, &connectionStatusBlock);
  //if (clientSetCbRet != IOTHUB_CLIENT_OK) {
  //  Log(F("set cb ret: "));
  //  LogLn(IOTHUB_CLIENT_RESULTStrings(clientSetCbRet));
  //}
  //while (!connectionStatusBlock.complete) {
  //  LogLn(F("Waiting for IoTHub connection to complete"));
  //  delay(200);
  //}
  //if (connectionStatusBlock.complete && connectionStatusBlock.status != IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
  //  hubStatusString = F("AIoTH: connect failed");
  //  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
  //  LogLn(hubStatusString);
  //  Log(F("  "));
  //  LogLn(IOTHUB_CLIENT_CONNECTION_STATUSStrings(connectionStatusBlock.status));
  //  Log(F("  "));
  //  LogLn(IOTHUB_CLIENT_CONNECTION_STATUS_REASONStrings(connectionStatusBlock.reason));
  //  WiFi.mode(WIFI_OFF);
  //  return;
  //}
  // really, really poor person's event system: reset event
  //connectionStatusBlock.complete = false;

  messageString = F("Creating message");
  LogLn(messageString);
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);

  String hubMessage = dataPoint.toString();
  IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString(hubMessage.c_str());
  if (message_handle == NULL) {
    messageString = F("Create failed.");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    WiFi.mode(WIFI_OFF);
    digitalWrite(GRN_LED, LOW);
    return;
  }

  messageString = F("Sending message");
  LogLn(messageString);
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);

  iothub_callback_status_block sb;
  sb.complete = false;
  int result = IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, message_handle, send_confirm_callback, &sb);
  IoTHubMessage_Destroy(message_handle); // per sample, this has been copied and can be destroyed
  if (result != IOTHUB_CLIENT_OK) {
    messageString = F("Send failed.");
    display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
    dataString = String(result, HEX);
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
    IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    WiFi.mode(WIFI_OFF);
    digitalWrite(GRN_LED, LOW);
    return;
  }

  IoTHubDeviceClient_LL_DoWork(device_ll_handle);

  // wait for message send completion callback
  //const int MSG_WAIT_COUNT = 20;
  const int MSG_WAIT_DELAY = 500; // ms
  int msgWaitIdx = 0;
  messageString = F("Waiting for send...");
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);
  while (!sb.complete) {
    msgWaitIdx += 1;
    dataString = F("Wait count: ");
    dataString += msgWaitIdx;
    display.writeLine(LinedDisplay::DATA_LINE, dataString);
    delay(MSG_WAIT_DELAY);
  }
  Log(F("Got IoT Hub callback, "));
  LogLn(IOTHUB_CLIENT_CONFIRMATION_RESULTStrings(sb.result));

  if (sb.result == 0) {
    messageString = F("Message sent");
  } else {
    messageString = F("Send failed");
  }
  LogLn(messageString);
  display.writeLine(LinedDisplay::MESSAGE_LINE, messageString);

  //
  // Celebrate a job well done.
  //

  // Cleanup
  IoTHubDeviceClient_LL_Destroy(device_ll_handle);
  WiFi.mode(WIFI_OFF);
  LogLn(F("End of complete loop()"));
  digitalWrite(GRN_LED, LOW);
}
