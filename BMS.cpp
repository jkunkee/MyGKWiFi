
#include "BMS.h"
#include <Arduino.h>
#include <Wire.h>

#include "Log.h"

static const byte bms_i2c_addr_7b = 0x43;
// max speed: 100kHz

class reg {
public:
  byte len;
  byte addr;
  boolean is_readonly;
  int def;
  int desired;

private:
  reg();

  byte unlock_code() {
    return ((bms_i2c_addr_7b << 1) ^ 0xC9 ^ addr);
  }

public:
  reg(byte _len, byte _addr, boolean _is_readonly, int _def, int _desired) :
    len(_len), addr(_addr), is_readonly(_is_readonly), def(_def), desired(_desired)
  {}

  boolean read(int *out) {
    if (out == NULL) {
      LogLn(F("Can't write data to NULL"));
      return false;
    }
    Wire.beginTransmission(bms_i2c_addr_7b);
    Wire.write(addr);
    int txResult = Wire.endTransmission(false);
    if (txResult != 0) {
      if (txResult == 2) {
        LogLnVerbose("Read setup failed due to NACK on ADDR. Device probably not present.");
      } else {
        Log(F("Read setup failed: "));
        LogLn(String(txResult));
      }
      return false;
    }
    byte count = Wire.requestFrom(bms_i2c_addr_7b, len);
    if (count < len) {
      Log(F("Read returned too few bytes: "));
      Log(count);
      Log(F("/"));
      LogLn(len);
      return false;
    }
    if (count > sizeof(*out)) {
      LogLn(F("Value read will not fit output datatype"));
      return false;
    }
    int shiftBits = 0;
    *out = 0;
    while (Wire.available()) {
      *out += ((unsigned int)(Wire.read()) << shiftBits);
      shiftBits += 8;
    }
    LogVerbose(F("Read at "));
    LogVerbose(addr);
    LogVerbose(F(" got "));
    LogLnVerbose(*out);
    return true;
  }

  boolean write(int val) {
    byte message[4]; // largest possible message
    Wire.beginTransmission(bms_i2c_addr_7b);
    Wire.write(addr);
    Wire.write(unlock_code());
    // register is little-endian, so write LSB first
    for (int byteIdx = 0; byteIdx < len; byteIdx++) {
      byte b = (val >> (8*byteIdx)) & 0xFF;
      Wire.write(b);
    }
    int txResult = Wire.endTransmission();
    if (txResult == 0) {
      return true;
    } else {
#ifdef DEBUG_LOGGING_VERBOSE
      Log(F("BMS: Setting register "));
      Log(String(addr, HEX));
      Log(F(" to "));
      Log(String(val, HEX));
      Log(F(" failed with "));
      LogLn(txResult);
#endif // DEBUG_LOGGING_VERBOSE
      return false;
    }
  }

  boolean writeDefault() {
    return write(def);
  }

  boolean writeDesired() {
    return write(desired);
  }
};

static reg I2C_REG_VER(1, 0x00, true,  0x07, 0x07);
static reg I2C_ADDRESS(1, 0x01, false, 0x43, 0x43);

static reg LED_STATE  (1, 0x02, false, 0x01, 0x01);

static reg TOUCH_STATE(1, 0x3A, true,  0x00, 0x00);
static reg TOUCH_CAP_CYCLES(1, 0x03, false, 0x00, 0x00);
static reg TOUCH_THRESHOLD(1, 0x04, false, 12, 12);
static reg TOUCH_HYSTERESIS(1, 0x05, false, 2, 2);

// uC factory-calibrated values
// static reg DCO_RSEL(1, 0x06, false, 0, 0); // factory-calibrated!
// static reg DCO_DCOMOD(1, 0x07, false, 0, 0); // factory-calibrated!

static reg VIN(2, 0x36, true, 0x00, 0x00);
static reg VBAT(2, 0x32, true, 0x00, 0x00);
static reg VOUT(2, 0x34, true, 0x00, 0x00);
static reg IOUT(2, 0x38, true, 0x00, 0x00);

static reg VBAT_MIN(2, 0x08, false, 4665, 4665); // 4665==2.85V
static reg VBAT_SHDN(2, 0x0a, false, 4829, 4829); // 4829==2.95V
static reg VBAT_BOOT(2, 0x0c, false, 5156, 5156); // 5156==3.15V
static reg VOUT_MAX(2, 0x0e, false, 5449, 5449); // 5449==3.5V
static reg VIN_THRESHOLD(2, 0x10, false, 1451, 1451); // 1451==4.5V
static reg IOUT_SHDN_THRESH(2, 0x1a, false, 0, 0); // 0==off

// battery factory-calibrated value
//static reg VBAT_OFFSET(2, 0x012, false, 0, 0); // factory-calibrated!
//static reg VOUT_OFFSET(2, 0x14, false, 0, 0); // factory-calibrated!
//static reg VIN_OFFSET(2, 0x16, false, 0, 0); // factory-calibrated!
static reg IOUT_OFFSET(2, 0x18, false, 0, 0); // actual default is 0

#define AUTO_BOOT_OFF 0x00
#define AUTO_BOOT_VBAT 0x01
#define AUTO_BOOT_VBAT_SMART 0x02
#define AUTO_BOOT_VIN 0x03
#define AUTO_BOOT_VIN_SMART 0x04
#define AUTO_BOOT_NO_VIN 0x05
#define AUTO_BOOT_NO_VIN_SMART 0x06

static reg AUTO_BOOT(1, 0x20, false, AUTO_BOOT_OFF, AUTO_BOOT_VIN);

static reg WAKE_TIME(2, 0x26, false, 0, 0);
static reg SHDN_DELAY(2, 0x1c, false, 65, 65); // ticks, 8 ticks/second
static reg AUTO_SHDN_TIME(2, 0x1e, false, 0xFFFF, 0xFFFF); // 0xFFFF==off

static reg PI_BOOT_TO(1, 0x21, false, 30, 0); // 0==don't timeout if not acknowledged
static reg PI_SHDN_TO(1, 0x22, false, 12, 0); // 0==don't timeout if not acknowledged

static reg RTC_TIME(4, 0x28, false, 0, 0); // SHINY
static reg RTC_WAKE_TIME(4, 0x2C, false, 0, 0);

#define WATCHDOG_OFF 0x00
#define WATCHDOG_ALERT 0x01
#define WATCHDOG_SHDN 0x02
static reg WATCHDOG_CFG(1, 0x23, false, 0x00, 0x00);
static reg WATCHDOG_GRACE(1, 0x24, false, 2, 2);
static reg WATCHDOG_TIMER(1, 0x30, false, 2, 2); // initialized to WATCHDOG_GRACE on Pi boot

static reg PI_RUNNING(1, 0x31, false, 1, 1); // gets set to 1 when Pi has booted by the daemon
// SHINY

static reg CFG_WRITE(1, 0x25, false, 0, 0); // write 0x46 to get commit flash-based settings to flash

static boolean bms_reconfigure() {
  boolean retVal = true;
  retVal = retVal && AUTO_BOOT.writeDesired();
  retVal = retVal && PI_BOOT_TO.writeDesired();
  retVal = retVal && PI_SHDN_TO.writeDesired();
  return retVal;
}

static boolean dump_reg(reg& r) {
  int val = 0;
  boolean retVal = r.read(&val);
  Log(F("Reg "));
  Log(String(r.addr, HEX));
  Log(F(": "));
  LogLn(String(val, HEX));
  return retVal;
}

static boolean bms_dump_config() {
  boolean retVal = true;
  retVal = retVal && dump_reg(AUTO_BOOT);
  retVal = retVal && dump_reg(PI_BOOT_TO);
  retVal = retVal && dump_reg(PI_SHDN_TO);
  retVal = retVal && dump_reg(RTC_TIME);
  return retVal;
}

static boolean bms_is_wired;

boolean bms_is_shutdown_requested() {
  if (!bms_is_wired) return false;
  int val = 0;
  boolean success = PI_RUNNING.read(&val);
  return (success && val != 1);
}

static int shutdown_complete_pin;

void bms_notify_shutdown_complete() {
  if (!bms_is_wired) return;
  // set Pi GPIO14 to LOW
  digitalWrite(shutdown_complete_pin, LOW);
}

void bms_notify_boot_complete() {
  if (!bms_is_wired) return;
  PI_RUNNING.write(1);
}

void bms_init(int PiGpio14Pin) {
  shutdown_complete_pin = PiGpio14Pin;
  pinMode(shutdown_complete_pin, OUTPUT);
  digitalWrite(shutdown_complete_pin, HIGH);

  int val = 0;
  boolean success = I2C_REG_VER.read(&val);
  bms_is_wired = success;
  if (!success) {
    // Normal case: BMS not wired, read fails, move on
    LogLn(F("BMS not connected, not verifying configuration"));
    return;
  }
  success = bms_reconfigure();
  if (!success) {
    LogLn(F("BMS did not accept reconfiguration"));
    return;
  }
  success = bms_dump_config();
  if (!success) {
    LogLn(F("BMS failed to dump configuration"));
    return;
  }
}
