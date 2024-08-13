#ifndef MACROS_H
#define MACROS_H

#ifdef DEBUG
#define PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#endif
