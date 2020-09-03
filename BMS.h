
#pragma once

#include <Arduino.h>

void bms_init(int PiGpio14Pin);
void bms_notify_boot_complete();
boolean bms_is_shutdown_requested();
void bms_notify_shutdown_complete();
