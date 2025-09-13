#pragma once
#include <Arduino.h>

#ifdef DEBUG
#define DBG_LED_GREEN 2
#define DBG_LED_RED 3

#ifndef DEBUG_SERIAL_TIMEOUT_MS
#define DEBUG_SERIAL_TIMEOUT_MS 3000 // wait up to 3 s for USB-CDC
#endif
#define DBG_BEGIN(baud)                                               \
    do                                                                \
    {                                                                 \
        Serial.begin(baud);                                           \
        uint32_t _t0 = millis();                                      \
        while (!Serial && (millis() - _t0) < DEBUG_SERIAL_TIMEOUT_MS) \
        {                                                             \
            delay(10);                                                \
        }                                                             \
        delay(100); /* small settle so first prints aren’t lost */    \
    } while (0)

#define DBG_PRINT(...) Serial.printf(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)

#define DBG_GUARD(name, reset_condition)   \
    static bool __dbg_once_##name = false; \
    do                                     \
    {                                      \
        if (reset_condition)               \
            __dbg_once_##name = false;     \
    } while (0)

#define DBG_GUARDED_PRINTF(name, ...)   \
    do                                  \
    {                                   \
        if (!__dbg_once_##name)         \
        {                               \
            __dbg_once_##name = true;   \
            Serial.printf(__VA_ARGS__); \
        }                               \
    } while (0)

#define DBG_LED_INIT()                    \
    do                                    \
    {                                     \
        pinMode(DBG_LED_GREEN, OUTPUT);   \
        digitalWrite(DBG_LED_GREEN, LOW); \
        pinMode(DBG_LED_RED, OUTPUT);     \
        digitalWrite(DBG_LED_RED, LOW);   \
    } while (0)

#define DBG_LED_GREEN_ON() digitalWrite(DBG_LED_GREEN, HIGH)
#define DBG_LED_GREEN_OFF() digitalWrite(DBG_LED_GREEN, LOW)
#define DBG_LED_GREEN_TOGGLE() digitalWrite(DBG_LED_GREEN, !digitalRead(DBG_LED_GREEN))

#define DBG_LED_RED_ON() digitalWrite(DBG_LED_RED, HIGH)
#define DBG_LED_RED_OFF() digitalWrite(DBG_LED_RED, LOW)

#else // DEBUG
#define DBG_BEGIN(baud) \
    do                  \
    {                   \
    } while (0)
#define DBG_PRINT(...) \
    do                 \
    {                  \
    } while (0)
#define DBG_PRINTLN(...) \
    do                   \
    {                    \
    } while (0)

#define DBG_LED_INIT() \
    do                 \
    {                  \
    } while (0)
#define DBG_LED_GREEN_ON() \
    do                     \
    {                      \
    } while (0)
#define DBG_LED_GREEN_OFF() \
    do                      \
    {                       \
    } while (0)
#define DBG_LED_GREEN_TOGGLE() \
    do                         \
    {                          \
    } while (0)
#define DBG_LED_RED_ON() \
    do                   \
    {                    \
    } while (0)
#define DBG_LED_RED_OFF() \
    do                    \
    {                     \
    } while (0)
#define DBG_GUARD(...) \
    do                 \
    {                  \
    } while (0)
#define DBG_GUARDED_PRINTF(...) \
    do                          \
    {                           \
    } while (0)
#endif // DEBUG