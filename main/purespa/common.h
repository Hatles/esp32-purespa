#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <limits.h>
#include "driver/gpio.h"

/*****************************************************************************
                       C O N F I G U R A T I O N
 *****************************************************************************/

// Select Intex PureSpa model
#define MODEL_SB_H20
//#define MODEL_SJB_HS

// Custom model name
//#define CUSTOM_MODEL_NAME "Intex PureSpa"

// Force WiFi sleep during critical operations (optional)
//#define FORCE_WIFI_SLEEP

//#define SERIAL_DEBUG

/*****************************************************************************/

namespace CONFIG
{
  static const char WIFI_VERSION[] = "1.0.8.2-ESP32";

  // WiFi parameters
  static const unsigned long WIFI_MAX_DISCONNECT_DURATION = 900000; // [ms] 5 min until reboot

  // MQTT publish rates
  static const unsigned int  POOL_UPDATE_PERIOD           =    500; // [ms]
  static const unsigned int  WIFI_UPDATE_PERIOD           =  30000; // [ms] 30 sec
  static const unsigned int  FORCED_STATE_UPDATE_PERIOD   =  10000; // [ms] 10 sec
}

// MQTT topics
namespace MQTT_TOPIC
{
  // publish
  static const char BUBBLE[]       = "pool/bubble";
  static const char DISINFECTION[] = "pool/disinfection"; // SJB-HS only
  static const char ERROR[]        = "pool/error";
  static const char FILTER[]       = "pool/filter";
  static const char HEATER[]       = "pool/heater";
  static const char JET[]          = "pool/jet"; // SJB-HS only
  static const char MODEL[]        = "pool/model";
  static const char POWER[]        = "pool/power";
  static const char WATER_ACT[]    = "pool/water/tempAct";
  static const char WATER_SET[]    = "pool/water/tempSet";
  static const char VERSION[]      = "wifi/version";
  static const char IP[]           = "wifi/ip";
  static const char RSSI[]         = "wifi/rssi";
  static const char WIFI_TEMP[]    = "wifi/temp";
  static const char STATE[]        = "wifi/state";
  static const char OTA[]          = "wifi/update";

  // subscribe
  static const char CMD_BUBBLE[]       = "pool/command/bubble";
  static const char CMD_DISINFECTION[] = "pool/command/disinfection"; // SJB-HS only
  static const char CMD_FILTER[]       = "pool/command/filter";
  static const char CMD_HEATER[]       = "pool/command/heater";
  static const char CMD_JET[]          = "pool/command/jet"; // SJB-HS only
  static const char CMD_POWER[]        = "pool/command/power";
  static const char CMD_WATER[]        = "pool/command/water/tempSet";
  static const char CMD_OTA[]          = "wifi/command/update";
}

// Languages
enum class LANG
{
  CODE = 0, EN = 1, DE = 2
};

// ESP32 pins
namespace PIN
{
  static const gpio_num_t CLOCK = GPIO_NUM_18;
  static const gpio_num_t DATA  = GPIO_NUM_19;
  static const gpio_num_t LATCH = GPIO_NUM_21;
}

// Debug macro
#ifdef SERIAL_DEBUG
#include "esp_log.h"
#define DEBUG_MSG(...) ESP_LOGI("PureSpa", __VA_ARGS__)
#else
#define DEBUG_MSG(...)
#endif

// Helper functions for time diff
static inline unsigned long timeDiff(unsigned long newTime, unsigned long oldTime)
{
  if (newTime >= oldTime)
  {
    return newTime - oldTime;
  }
  else
  {
    return ULONG_MAX - oldTime + newTime + 1;
  }
}

static inline unsigned long diff(unsigned int newVal, unsigned int oldVal)
{
  if (newVal >= oldVal)
  {
    return newVal - oldVal;
  }
  else
  {
    return UINT_MAX - oldVal + newVal + 1;
  }
}

#endif /* COMMON_H */
