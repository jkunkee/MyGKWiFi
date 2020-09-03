
//#define MYGKWIFI_DEBUG

#include "gkplus_datapoint.h"
#include "Credentials.h"
#include "BMS.h"
#include "Log.h"

#include <AzureIoTHub.h>
#include "AzureIoTProtocol_HTTP.h"
#include "iothubtransporthttp.h"

// sample_init bits from Azure SDK
// Times before 2010 (1970 + 40 years) are invalid
#include <time.h>
#define MIN_EPOCH (40 * 365 * 24 * 3600)

// Work in GK-WiFi_v2_0 includes

#include <ESP8266WiFi.h>
#include "SSD1306.h"                    // OLED display library - Latest Git commit for clockspeed fixes
//                                        https://github.com/squix78/esp8266-oled-ssd1306
#include "OLEDDisplayFonts.h"             // additional fonts in separate file in lib
#include <Wire.h>                       // for I2C

//----------------------------------------------------------------------------------------------+
//                               PIN MAP for ESP8266
//----------------------------------------------------------------------------------------------+
#define SDA_PIN        0                // I2C SDA pin (pulled up) + PGM button and LED
// TX                  1                // wired to serial comm
#define SCL_PIN        2                // I2C SCL pin (pulled up)
// RX                  3                // wired to serial comm
#define PI_GPIO_14     4                // line to BMS for notifying that shutdown is complete
// not used            5                // reserved
#define YEL_LED        12               // steady = empty buffer (no GK connection yet)
#define RED_LED        13               // connecting to network - off when connected.
#define GRN_LED        14               // flashes with each send
// RUN / PGM           15               // pulled low on carrier board
// WAKE                16               // wired to RESET through cap to wake from sleep

//SSD1306 display(0x3c, SDA_PIN, SCL_PIN); // OLED address (0x3c), SDA (GPIO0), SCL(GPIO2)
class LinedDisplay {

private:
  class Line {
  public:
    String text;
    OLEDDISPLAY_TEXT_ALIGNMENT alignment;
    const uint8_t* font;
    int font_height;

    Line() : text(""), alignment(TEXT_ALIGN_CENTER) {
      setFont(ArialMT_Plain_16);
    }

    void setFont(const uint8_t *newFont) {
      if (newFont != NULL) {
        font = newFont;
        font_height = font[HEIGHT_POS]; // Arial 10pt==13px, 16pt==19px, 24pt==28px
      }
    }
  };

  SSD1306 screen;
  const int16_t screen_rows = 64;
  const int16_t screen_columns = 128;
  Line *lines;

public:
  enum LineNumber { BANNER_LINE = 0, PHASE_LINE, MESSAGE_LINE, LINE_COUNT };

  LinedDisplay() : screen(0x3c, SDA_PIN, SCL_PIN, GEOMETRY_128_64, I2C_ONE, 100000) {
    lines = new Line[LINE_COUNT];
    // BANNER and PHASE are Arial 16, the Line default
    lines[MESSAGE_LINE].setFont(ArialMT_Plain_10);
  }

  ~LinedDisplay() {
    screen.end();
    delete(lines);
  }

  void begin() {
    screen.init();
    screen.flipScreenVertically();       // display pins on top
    screen.setContrast(255);
  }

  void splash() {
    lines[BANNER_LINE].text = "GK-WiFi vMine";
    lines[PHASE_LINE].text = "Built: " __TIME__;
    lines[MESSAGE_LINE].text = "Booting ...";
  }

  void writeLine(LineNumber lineIdx, String& s) {
    if (lineIdx < 0 || LINE_COUNT <= lineIdx) {
      return;
    }

    lines[lineIdx].text = s;

    paint();
  }

  void paint() {
    screen.clear();
    int targetRow = 0;
    for (int lineIdx = 0; lineIdx < LINE_COUNT; lineIdx++) {
      Line *line = &lines[lineIdx];
      screen.setFont(line->font);
      screen.setTextAlignment(line->alignment);
      // TODO: handle different alignments
      screen.drawString(64, targetRow, line->text.c_str());
      targetRow += line->font_height;
    }
    screen.display();
  }

  void testPattern() {
    screen.setColor(WHITE);
    screen.clear();
    for (int16_t colIdx = 0; colIdx < screen_columns; colIdx++) {
      for (int16_t rowIdx = 0; rowIdx < screen_rows; rowIdx++) {
        if (colIdx % 2 == 0 && rowIdx % 2 == 0) {
          screen.setPixel(colIdx, rowIdx);
        }
      }
    }
    screen.display();
  }
};
LinedDisplay display;

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

  bms_notify_boot_complete();

  digitalWrite(RED_LED, LOW);
}

void loop() {
  static String serialBuffer("");
  static boolean serialLineComplete = false;
  static int loopCount = 0;
  String serialLine("");

  loopCount += 1;
  // Don't spam the I2C line
  if (loopCount > 1000) {
    loopCount = 0;
    if (bms_is_shutdown_requested()) {
      bms_notify_shutdown_complete();
      delay(1000);
      return;
    }
  }

  while (Serial.available()) {
    digitalWrite(YEL_LED, HIGH);
    char inChar = (char)(Serial.read());
    if (inChar == '\n' || inChar == '\r') {
      serialLineComplete = true;
      serialLine = serialBuffer;
      serialBuffer = "";
      Log("Got line: '");
      Log(serialLine);
      LogLn("'\n");
      break;
    } else {
      serialBuffer += inChar;
    }
  }

  if (serialLineComplete) {
    digitalWrite(YEL_LED, LOW);
    serialLineComplete = false;
  } else {
#ifdef MYGKWIFI_DEBUG
    delay(1000);
    digitalWrite(YEL_LED, LOW);
    serialLine = gkplus_datapoint.getFakeLine();
#else // MYGKWIFI_DEBUG
    return;
#endif // MYGKWIFI_DEBUG
  }
  serialLine.trim();

  // If for some reason we get empty lines, ignore them.
  // One case is \r\n being parsed as two line endings.
  if (serialLine.length() == 0) {
    LogLn("Failed zero-length check");
    return;
  }

  gkplus_datapoint dataPoint(serialLine);
  boolean dataPointValid = dataPoint.initialized;
  Log("dataPoint valid? ");
  LogLn(dataPointValid);

  // write state to display
  if (dataPointValid) {
    String dataPointString = dataPoint.toString();
    display.writeLine(LinedDisplay::PHASE_LINE, dataPointString);
  } else {
    String failureString("Invalid datum");
    display.writeLine(LinedDisplay::PHASE_LINE, failureString);
    return;
  }

  // Start/connect WiFi
  String wifiString("WiFi starting...");
  display.writeLine(LinedDisplay::MESSAGE_LINE, wifiString);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  const int WIFI_WAIT_COUNT = 20;
  const int WIFI_WAIT_PERIOD = 500; // ms
  int waitIdx = 0;
  while (WiFi.status() != WL_CONNECTED && waitIdx < WIFI_WAIT_COUNT) {
    wifiString = "WiFi wait count: ";
    waitIdx += 1;
    wifiString += String(waitIdx/2);
    display.writeLine(LinedDisplay::MESSAGE_LINE, wifiString);
    delay(WIFI_WAIT_PERIOD);
  }
  if (WiFi.status() != WL_CONNECTED) {
    wifiString = "WiFi conn. failed: ";
    wifiString += WiFi.status();
    display.writeLine(LinedDisplay::MESSAGE_LINE, wifiString);
    Log(wifiString);
    WiFi.mode(WIFI_OFF);
    return;
  }
  wifiString = "WiFi connected.";
  digitalWrite(GRN_LED, HIGH);
  display.writeLine(LinedDisplay::MESSAGE_LINE, wifiString);

  String hubStatusString;
  
  // Time needs to be approximately correct for the sample to work,
  // probably because of the hub or SSL/TLS.
  // This is copy-pasted from sample_init.cpp in the Azure SDK and tweaked a little.
  hubStatusString = "Syncing Time";
  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
  {
   time_t epochTime;

   configTime(0, 0, "pool.ntp.org", "time.nist.gov");
   delay(2000);

   while (true) {
       epochTime = time(NULL);

       if (epochTime < MIN_EPOCH) {
           LogLn("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
           delay(2000);
       } else {
           Log("Fetched NTP epoch time is: ");
           LogLn(epochTime);
           break;
       }
   }
  }

  // Construct message, connect to IoT Hub, send message
  hubStatusString = "AIoTH: creating client";
  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);

  IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;
  const IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = HTTP_Protocol; // HTTP_Protocol, MQTT_Protocol
  device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(AIH_CONN_STRING, protocol);

  if (device_ll_handle == NULL) {
    hubStatusString = "AIoTH: create failed";
    display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
    WiFi.mode(WIFI_OFF);
    digitalWrite(GRN_LED, LOW);
    return;
  }

  // Setting the Trusted Certificate.
  IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

  // Example sdk status tracing for troubleshooting
  bool traceOn = true;
  IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &traceOn);

  // Setting connection status callback to get indication of connection to iothub
  // Without multithreading and an easy-to-write state machine, monitoring the connection
  // status is not easy enough to do here. Instead only rely on it being stable for one
  // message.
  //iothub_conn_callback_status_block connectionStatusBlock;
  //connectionStatusBlock.complete = false;
  //IOTHUB_CLIENT_RESULT clientSetCbRet = IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, connection_status_callback, &connectionStatusBlock);
  //if (clientSetCbRet != IOTHUB_CLIENT_OK) {
  //  Log("set cb ret: ");
  //  LogLn(IOTHUB_CLIENT_RESULTStrings(clientSetCbRet));
  //}
  //while (!connectionStatusBlock.complete) {
  //  LogLn("Waiting for IoTHub connection to complete");
  //  delay(200);
  //}
  //if (connectionStatusBlock.complete && connectionStatusBlock.status != IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
  //  hubStatusString = "AIoTH: connect failed";
  //  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
  //  LogLn(hubStatusString);
  //  Log("  ");
  //  LogLn(IOTHUB_CLIENT_CONNECTION_STATUSStrings(connectionStatusBlock.status));
  //  Log("  ");
  //  LogLn(IOTHUB_CLIENT_CONNECTION_STATUS_REASONStrings(connectionStatusBlock.reason));
  //  WiFi.mode(WIFI_OFF);
  //  return;
  //}
  // really, really poor person's event system: reset event
  //connectionStatusBlock.complete = false;

  hubStatusString = "AIoTH: creating message";
  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);

  String hubMessage = dataPoint.toString();
  IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString(hubMessage.c_str());
  if (message_handle == NULL) {
    hubStatusString = "AIoTH: create failed";
    display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
    IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    WiFi.mode(WIFI_OFF);
    digitalWrite(GRN_LED, LOW);
    return;
  }

  hubStatusString = "AIoTH: sending message";
  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);

  iothub_callback_status_block sb;
  sb.complete = false;
  int result = IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, message_handle, send_confirm_callback, &sb);
  IoTHubMessage_Destroy(message_handle); // per sample, this has been copied and can be destroyed
  if (result != IOTHUB_CLIENT_OK) {
    hubStatusString = "AIoTH: send failed";
    hubStatusString += String(result, HEX);
    display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
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
  while (!sb.complete) {
    msgWaitIdx += 1;
    hubStatusString = "Msg wait: ";
    hubStatusString += msgWaitIdx;
    display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);
    delay(MSG_WAIT_DELAY);
  }
  Log("Got callback, ");
  LogLn(IOTHUB_CLIENT_CONFIRMATION_RESULTStrings(sb.result));

  if (sb.result == 0) {
    hubStatusString = "AIoTH: message sent";
  } else {
    hubStatusString = "AIoTH: send failed";
  }
  display.writeLine(LinedDisplay::MESSAGE_LINE, hubStatusString);

  // Cleanup
  IoTHubDeviceClient_LL_Destroy(device_ll_handle);
  WiFi.mode(WIFI_OFF);
  digitalWrite(GRN_LED, LOW);
}
