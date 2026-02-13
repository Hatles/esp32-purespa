#ifndef PURE_SPA_IO_H
#define PURE_SPA_IO_H

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <string>
#include "common.h"
#include "esp_attr.h"

// Types missing in standard headers
typedef int32_t sint32;

class PureSpaIO
{
public:
  enum MODEL
  {
    SBH20 = 1,
    SJBHS = 2
  };

  class UNDEF
  {
  public:
    static const uint8_t  BOOL   = UCHAR_MAX;
    static const uint16_t USHORT = USHRT_MAX;
    static const uint32_t UINT   = UINT_MAX;
    static const sint32   INT    = -99;
  };

  class WATER_TEMP
  {
  public:
    static const int SET_MIN = 20; // °C
    static const int SET_MAX = 40; // °C
  };

public:
  void setup(LANG language);
  void loop();

public:
  MODEL getModel() const;
  const char* getModelName() const;

  bool isOnline() const;

  int getActWaterTempCelsius() const;
  int getDesiredWaterTempCelsius() const;
  int getDisinfectionTime() const;

  uint8_t isBubbleOn() const;
  uint8_t isBuzzerOn() const;
  uint8_t isDisinfectionOn() const;
  uint8_t isFilterOn() const;
  uint8_t isHeaterOn() const;
  uint8_t isHeaterStandby() const;
  uint8_t isJetOn() const;
  uint8_t isPowerOn() const;

  void setDesiredWaterTempCelsius(int temp);
  void setDisinfectionTime(int hours);

  void setBubbleOn(bool on);
  void setFilterOn(bool on);
  void setHeaterOn(bool on);
  void setJetOn(bool on);
  void setPowerOn(bool on);

  std::string getErrorCode() const;
  std::string getErrorMessage(const std::string& errorCode) const;

  unsigned int getRawLedValue() const;

  unsigned int getTotalFrames() const;
  unsigned int getDroppedFrames() const;

private:
  class CYCLE
  {
  public:
#if defined MODEL_SB_H20
    static const unsigned int BUTTON_FRAMES = 7;
#elif defined MODEL_SJB_HS
    static const unsigned int BUTTON_FRAMES = 9;
#endif
    static const unsigned int TOTAL_FRAMES = 25 + BUTTON_FRAMES;
    static const unsigned int DISPLAY_FRAME_GROUPS =  5;
    static const unsigned int PERIOD = 21; // ms
    static const unsigned int RECEIVE_TIMEOUT = 50*CYCLE::PERIOD; // ms
  };

  class FRAME
  {
  public  :
    static const unsigned int BITS = 16;
    static const unsigned int FREQUENCY = CYCLE::TOTAL_FRAMES/CYCLE::PERIOD;
  };

  class BLINK
  {
  public:
    static const unsigned int PERIOD = 500; // ms
    static const unsigned int TEMP_FRAMES = PERIOD/4*FRAME::FREQUENCY;
    static const unsigned int STOPPED_FRAMES = 2*PERIOD*FRAME::FREQUENCY;
  };

  class CONFIRM_FRAMES
  {
  public:
    static const unsigned int REGULAR = 3;
    static const unsigned int NOT_BLINKING = BLINK::PERIOD/2*FRAME::FREQUENCY/CYCLE::DISPLAY_FRAME_GROUPS;
  };

  class BUTTON
  {
  public:
    static const unsigned int PRESS_COUNT = BLINK::PERIOD/CYCLE::PERIOD;
    static const unsigned int PRESS_SHORT_COUNT = 380/CYCLE::PERIOD;
    static const unsigned int ACK_CHECK_PERIOD = 10; // ms
    static const unsigned int ACK_TIMEOUT = 2*PRESS_COUNT*CYCLE::PERIOD; // ms
  };

private:
  struct State
  {
    uint32_t waterTemp        = UNDEF::UINT;
    uint32_t desiredTemp      = UNDEF::UINT;
    uint32_t disinfectionTime = UNDEF::UINT;
    uint32_t error            = 0;

    uint16_t ledStatus        = UNDEF::USHORT;

    bool buzzer = false;
    bool online = false;
    bool stateUpdated = false;

    unsigned int lastErrorChangeFrameCounter = 0;
    unsigned int frameCounter = 0;
    unsigned int frameDropped = 0;
  };

  struct IsrState
  {
    uint32_t latestWaterTemp        = UNDEF::UINT;
    uint32_t latestBlinkingTemp     = UNDEF::UINT;
    uint32_t latestDisinfectionTime = UNDEF::UINT;
    uint16_t latestLedStatus = UNDEF::USHORT;

    uint16_t frameValue = 0;
    uint16_t receivedBits = 0;

    unsigned int lastBlankDisplayFrameCounter = 0;
    unsigned int blankCounter = 0;

    unsigned int stableDisplayValueCount      = CONFIRM_FRAMES::REGULAR;
    unsigned int stableDisplayBlankCount      = CONFIRM_FRAMES::REGULAR;
    unsigned int stableWaterTempCount         = CONFIRM_FRAMES::NOT_BLINKING;
    unsigned int stableBlinkingWaterTempCount = 0;
    unsigned int stableDisinfectionTimeCount  = CONFIRM_FRAMES::REGULAR;
    unsigned int stableLedStatusCount         = CONFIRM_FRAMES::REGULAR;

    uint32_t displayValue       = UNDEF::UINT;
    uint32_t latestDisplayValue = UNDEF::UINT;

    uint8_t receivedDigits = 0;

    bool isDisplayBlinking = false;

    bool reply = false;
  };

  struct Buttons
  {
    unsigned int toggleBubble       = 0;
    unsigned int toggleDisinfection = 0;
    unsigned int toggleFilter       = 0;
    unsigned int toggleHeater       = 0;
    unsigned int toggleJet          = 0;
    unsigned int togglePower        = 0;
    unsigned int toggleTempUp       = 0;
    unsigned int toggleTempDown     = 0;
  };

private:
  // ISR and ISR helper
  static IRAM_ATTR void clockRisingISR(void* arg);
  static IRAM_ATTR void decodeDisplay();
  static IRAM_ATTR void decodeLED();
  static IRAM_ATTR void decodeButton();
  static IRAM_ATTR void updateButtonState(volatile unsigned int& buttonPressCount);

private:
  // ISR variables
  static volatile State state;
  static volatile IsrState isrState;
  static volatile Buttons buttons;

private:
  int convertDisplayToCelsius(uint32_t value) const;
  bool waitBuzzerOff() const;
  bool pressButton(volatile unsigned int& buttonPressCount);
  bool changeWaterTemp(int up);

private:
#if defined MODEL_SB_H20
  MODEL model = MODEL::SBH20;
#elif defined MODEL_SJB_HS
  MODEL model = MODEL::SJBHS;
#else
  MODEL model;
#endif

private:
  LANG language;
  unsigned long lastStateUpdateTime = 0;
  char errorBuffer[4];
};

#endif /* PURE_SPA_IO_H */
