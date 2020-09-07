
#pragma once

#include <Arduino.h>

//#define DEBUG_LOGGING
//#define DEBUG_LOGGING_VERBOSE

#if defined(DEBUG_LOGGING)

#define Log(x) Serial.print(x)
#define LogFile(x) { Log(__FILE__); Log(": "); Log(x); }
#define LogFileFunc(x) { Log(__FILE__); Log(" "); Log(__func__); Log(": "); LogFile(x); }
#define LogLn(x) Serial.println(x)
#define LogLnFile(x) { LogFile(x); LogLn(""); }
#define LogLnFileFunc(x) { LogFileFunc(x); LogLn(""); }

#if defined(DEBUG_LOGGING_VERBOSE)

#define LogVerbose(x) LogFileFunc(x)
#define LogLnVerbose(x) LogLnFileFunc(x)

#else // DEBUG_LOGGING_VERBOSE

#define LogVerbose(x)
#define LogLnVerbose(x)

#endif // DEBUG_LOGGING_VERBOSE

#else // DEBUG_LOGGING

#define Log(x)
#define LogFile(x)
#define LogFileFunc(x)
#define LogLn(x)
#define LogLnFile(x)
#define LogLnFileFunc(x)

#define LogVerbose(x)
#define LogLnVerbose(x)

#endif // DEBUG_LOGGING
