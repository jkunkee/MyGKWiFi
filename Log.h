
#pragma once

#include <Arduino.h>

//#define DEBUG_LOGGING
//#define DEBUG_LOGGING_VERBOSE

#if defined(DEBUG_LOGGING)

#define Log(x) Serial.print(x)
#define LogLn(x) Serial.println(x)

#if defined(DEBUG_LOGGING_VERBOSE)

#define LogLnVerbose(x) LogLn(x)
#define LogLnFile(x) { Log(__FILE__); Log(": "); LogLn(x); }
#define LogLnFileFunc(x) { Log(__FILE__); Log(" "); Log(__func__); Log(": "); LogLnFile(x); }

#else // DEBUG_LOGGING_VERBOSE

#define LogLnFile(x)
#define LogLnFileFunc(x)

#endif // DEBUG_LOGGING_VERBOSE

#else // DEBUG_LOGGING

#define Log(x)
#define LogLn(x)
#define LogLnFile(x)
#define LogLnFileFunc(x)

#endif // DEBUG_LOGGING
