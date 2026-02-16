#include "PureSpaIO.h"
#include <esp_timer.h>
#include <rom/ets_sys.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PureSpaIO";

#if defined MODEL_SB_H20
#define DEFAULT_MODEL_NAME "Intex PureSpa SB-H20"

namespace FRAME_LED {
  const uint16_t POWER          = 0x0001;
  const uint16_t HEATER_ON      = 0x0080;
  const uint16_t NO_BEEP        = 0x0100;
  const uint16_t HEATER_STANDBY = 0x0200;
  const uint16_t BUBBLE         = 0x0400;
  const uint16_t FILTER         = 0x1000;
}

namespace FRAME_BUTTON {
  const uint16_t FILTER    = 0x0002;
  const uint16_t BUBBLE    = 0x0008;
  const uint16_t TEMP_DOWN = 0x0080;
  const uint16_t POWER     = 0x0400;
  const uint16_t TEMP_UP   = 0x1000;
  const uint16_t TEMP_UNIT = 0x2000;
  const uint16_t HEATER    = 0x8000;
}

#elif defined MODEL_SJB_HS
#define DEFAULT_MODEL_NAME "Intex PureSpa SJB-HS"
#endif

#ifdef CUSTOM_MODEL_NAME
const char MODEL_NAME[] = CUSTOM_MODEL_NAME;
#else
const char MODEL_NAME[] = DEFAULT_MODEL_NAME;
#endif

namespace FRAME_DIGIT {
  const uint16_t POS_1 = 0x0040;
  const uint16_t POS_2 = 0x0020;
  const uint16_t POS_3 = 0x0800;
  const uint16_t POS_4 = 0x0004;

  const uint16_t SEGMENT_A  = 0x2000;
  const uint16_t SEGMENT_B  = 0x1000;
  const uint16_t SEGMENT_C  = 0x0200;
  const uint16_t SEGMENT_D  = 0x0400;
  const uint16_t SEGMENT_E  = 0x0080;
  const uint16_t SEGMENT_F  = 0x0008;
  const uint16_t SEGMENT_G  = 0x0010;
  const uint16_t SEGMENT_DP = 0x8000;
  const uint16_t SEGMENTS   = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F | SEGMENT_G;

  const uint16_t OFF   = 0x0000;
  const uint16_t NUM_0 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F;
  const uint16_t NUM_1 = SEGMENT_B | SEGMENT_C;
  const uint16_t NUM_2 = SEGMENT_A | SEGMENT_B | SEGMENT_G | SEGMENT_E | SEGMENT_D;
  const uint16_t NUM_3 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_G;
  const uint16_t NUM_4 = SEGMENT_F | SEGMENT_G | SEGMENT_B | SEGMENT_C;
  const uint16_t NUM_5 = SEGMENT_A | SEGMENT_F | SEGMENT_G | SEGMENT_C | SEGMENT_D;
  const uint16_t NUM_6 = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D | SEGMENT_C | SEGMENT_G;
  const uint16_t NUM_7 = SEGMENT_A | SEGMENT_B | SEGMENT_C;
  const uint16_t NUM_8 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F | SEGMENT_G;
  const uint16_t NUM_9 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_F | SEGMENT_G;
  const uint16_t LET_C = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D;
  const uint16_t LET_D = SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_G;
  const uint16_t LET_E = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D | SEGMENT_G;
  const uint16_t LET_F = SEGMENT_E | SEGMENT_F | SEGMENT_A | SEGMENT_G;
  const uint16_t LET_H = SEGMENT_B | SEGMENT_C | SEGMENT_E | SEGMENT_F | SEGMENT_G;
  const uint16_t LET_N = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_E | SEGMENT_F;
}

namespace FRAME_TYPE {
  const uint16_t CUE    = 0x0100;
  const uint16_t LED    = 0x4000;
  const uint16_t DIGIT  = FRAME_DIGIT::POS_1 | FRAME_DIGIT::POS_2 | FRAME_DIGIT::POS_3 | FRAME_DIGIT::POS_4;

#if defined MODEL_SB_H20
  const uint16_t BUTTON = CUE | FRAME_BUTTON::POWER | FRAME_BUTTON::FILTER | FRAME_BUTTON::HEATER | FRAME_BUTTON::BUBBLE | FRAME_BUTTON::TEMP_UP | FRAME_BUTTON::TEMP_DOWN | FRAME_BUTTON::TEMP_UNIT;
#elif defined MODEL_SJB_HS
  const uint16_t BUTTON = CUE | FRAME_BUTTON::POWER | FRAME_BUTTON::FILTER | FRAME_BUTTON::HEATER | FRAME_BUTTON::BUBBLE | FRAME_BUTTON::TEMP_UP | FRAME_BUTTON::TEMP_DOWN | FRAME_BUTTON::TEMP_UNIT | FRAME_BUTTON::DISINFECTION | FRAME_BUTTON::JET;
#endif
}

namespace DIGIT {
  const uint8_t POS_1     = 0x8;
  const uint8_t POS_2     = 0x4;
  const uint8_t POS_3     = 0x2;
  const uint8_t POS_4     = 0x1;
  const uint8_t POS_1_2   = POS_1 | POS_2;
  const uint8_t POS_1_2_3 = POS_1 | POS_2 | POS_3;
  const uint8_t POS_ALL   = POS_1 | POS_2 | POS_3 | POS_4;
  const char OFF = ' ';
}

namespace ERROR {
  const char CODE_90[]    = "E90";
  const char CODE_91[]    = "E91";
  const char CODE_92[]    = "E92";
  const char CODE_94[]    = "E94";
  const char CODE_95[]    = "E95";
  const char CODE_96[]    = "E96";
  const char CODE_97[]    = "E97";
  const char CODE_99[]    = "E99";
  const char CODE_END[]   = "END";
  const char CODE_OTHER[] = "EXX";

  const unsigned int COUNT = 9;

  const char EN_90[]    = "no water flow";
  const char EN_91[]    = "salt level too low";
  const char EN_92[]    = "salt level too high";
  const char EN_94[]    = "water temp too low";
  const char EN_95[]    = "water temp too high";
  const char EN_96[]    = "system error";
  const char EN_97[]    = "dry fire protection";
  const char EN_99[]    = "water temp sensor error";
  const char EN_END[]   = "heating aborted after 72h";
  const char EN_OTHER[] = "error";

  const char DE_90[]    = "kein Wasserdurchfluss";
  const char DE_91[]    = "niedriges Salzniveau";
  const char DE_92[]    = "hohes Salzniveau";
  const char DE_94[]    = "Wassertemperatur zu niedrig";
  const char DE_95[]    = "Wassertemperatur zu hoch";
  const char DE_96[]    = "Systemfehler";
  const char DE_97[]    = "Trocken-Brandschutz";
  const char DE_99[]    = "Wassertemperatursensor defekt";
  const char DE_END[]   = "Heizbetrieb nach 72 h deaktiviert";
  const char DE_OTHER[] = "Störung";

  const char* const TEXT[3][COUNT+1] = {
    { CODE_90, CODE_91, CODE_92, CODE_94, CODE_95, CODE_96, CODE_97, CODE_99, CODE_END, CODE_OTHER },
    { EN_90,   EN_91,   EN_92,   EN_94,   EN_95,   EN_96,   EN_97,   EN_99,   EN_END,   EN_OTHER },
    { DE_90,   DE_91,   DE_92,   DE_94,   DE_95,   DE_96,   DE_97,   DE_99,   DE_END,   DE_OTHER }
  };
}

inline char display2LastDigit(uint32_t v) { return (v >> 24) & 0xFFU; }
inline uint16_t display2Num(uint32_t v)     { return (((v & 0xFFU) - '0')*100) + ((((v >> 8) & 0xFFU) - '0')*10) + (((v >> 16) & 0xFFU) - '0'); }
inline uint32_t display2Error(uint32_t v)   { return v & 0x00FFFFFFU; }
inline bool displayIsTemp(uint32_t v)     { return display2LastDigit(v) == 'C' || display2LastDigit(v) == 'F'; }
inline bool displayIsTime(uint32_t v)     { return display2LastDigit(v) == 'H'; }
inline bool displayIsError(uint32_t v)    { return (v & 0xFFU) == 'E'; }
inline bool displayIsBlank(uint32_t v)    { return (v & 0x00FFFFFFU) == (' ' << 16) + (' ' << 8) + ' '; }

volatile PureSpaIO::State PureSpaIO::state;
volatile PureSpaIO::IsrState PureSpaIO::isrState;
volatile PureSpaIO::Buttons PureSpaIO::buttons;

static unsigned long millis() {
    return (unsigned long)(esp_timer_get_time() / 1000);
}

static void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void delayMicroseconds(uint32_t us) {
    ets_delay_us(us);
}

void PureSpaIO::setup(LANG language)
{
  this->language = language;

  // Configure CLOCK as Interrupt Input (Rising Edge)
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  io_conf.pin_bit_mask = (1ULL << PIN::CLOCK);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);

  // Configure LATCH as Interrupt Input (Falling Edge)
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  io_conf.pin_bit_mask = (1ULL << PIN::LATCH);
  gpio_config(&io_conf);

  // Configure DATA as Input
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.pin_bit_mask = (1ULL << PIN::DATA);
  gpio_config(&io_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(PIN::CLOCK, PureSpaIO::clockRisingISR, this);
  //gpio_isr_handler_add(PIN::LATCH, PureSpaIO::latchRisingISR, this);
}

PureSpaIO::MODEL PureSpaIO::getModel() const
{
  return model;
}

const char* PureSpaIO::getModelName() const
{
  return MODEL_NAME;
}

void PureSpaIO::loop()
{
  unsigned long now = millis();

  if (state.stateUpdated)
  {
    if (!init) {
      init = true;
      ESP_LOGI(TAG, "PureSpaIO Initialized");
    }

    lastStateUpdateTime = now;
    state.online = true;
    state.stateUpdated = false;
  }
  else if (state.online && timeDiff(now, lastStateUpdateTime) > CYCLE::RECEIVE_TIMEOUT)
  {
    state.online = false;
  }
}

bool PureSpaIO::isOnline() const
{
  return state.online;
}

unsigned int PureSpaIO::getTotalFrames() const
{
  return state.frameCounter;
}

unsigned int PureSpaIO::getDroppedFrames() const
{
  return state.frameDropped;
}

int PureSpaIO::getActWaterTempCelsius() const
{
  return (state.waterTemp != UNDEF::UINT) ? convertDisplayToCelsius(state.waterTemp) : UNDEF::INT;
}

int PureSpaIO::getDesiredWaterTempCelsius() const
{
  return (state.desiredTemp != UNDEF::UINT) ? convertDisplayToCelsius(state.desiredTemp) : UNDEF::INT;
}

int PureSpaIO::getDisinfectionTime() const
{
  return isDisinfectionOn() ? (state.disinfectionTime != UNDEF::UINT ? display2Num(state.disinfectionTime) : UNDEF::INT) : 0;
}

std::string PureSpaIO::getErrorCode() const
{
  char buf[5];
  memcpy(buf, (void*)&state.error, 4);
  buf[4] = 0;
  return std::string(buf);
}

std::string PureSpaIO::getErrorMessage(const std::string& errorCode) const
{
  if (errorCode.length())
  {
    unsigned int errorIndex = UINT_MAX;
    for (unsigned int i=0; i<ERROR::COUNT; i++)
    {
      if (errorCode == ERROR::TEXT[0][i]) 
      {
        errorIndex = i;
        break;
      }
    }

    if (errorIndex != UINT_MAX)
    {
      return std::string(ERROR::TEXT[(unsigned int)language][errorIndex]);
    }
    else
    {
      return errorCode;
    }
  }
  else
  {
    return errorCode;
  }
}

unsigned int PureSpaIO::getRawLedValue() const
{
  return (state.ledStatus != UNDEF::USHORT) ? state.ledStatus : UNDEF::USHORT;
}

uint8_t PureSpaIO::isPowerOn() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::POWER) != 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isFilterOn() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::FILTER) != 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isBubbleOn() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::BUBBLE) != 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isHeaterOn() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & (FRAME_LED::HEATER_ON | FRAME_LED::HEATER_STANDBY)) != 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isHeaterStandby() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::HEATER_STANDBY) != 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isBuzzerOn() const
{
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::NO_BEEP) == 0) : UNDEF::BOOL;
}

uint8_t PureSpaIO::isDisinfectionOn() const
{
#ifdef MODEL_SJB_HS
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::DISINFECTION) != 0) : UNDEF::BOOL;
#else
  return false;
#endif
}

uint8_t PureSpaIO::isJetOn() const
{
#ifdef MODEL_SJB_HS
  return (state.ledStatus != UNDEF::USHORT) ? ((state.ledStatus & FRAME_LED::JET) != 0) : UNDEF::BOOL;
#else
  return false;
#endif
}

void PureSpaIO::setDesiredWaterTempCelsius(int temp)
{
  if (temp >= WATER_TEMP::SET_MIN && temp <= WATER_TEMP::SET_MAX)
  {
    if (isPowerOn() && state.error == 0)
    {
      if (!changeWaterTemp(-1))
      {
        changeWaterTemp(+1);
      }

      int sleep = 5*CYCLE::PERIOD;
      int changeTries = 3;
      int setTemp = UNDEF::INT;
      bool getActualSetpoint = true;
      do
      {
        int readTries = 4*BLINK::PERIOD/sleep;
        int newSetTemp = getActualSetpoint? UNDEF::INT : setTemp;
        
        if (getActualSetpoint)
        {
          waitBuzzerOff();
          delay(BLINK::PERIOD);
        }
        while (getActualSetpoint)
        {
          newSetTemp = getDesiredWaterTempCelsius();
          readTries--;
          getActualSetpoint = newSetTemp == setTemp && readTries;
          if (getActualSetpoint)
          {
            delay(sleep);
          }
        }

        if (newSetTemp == UNDEF::INT)
        {
          return;
        }
        else
        {
          if (setTemp == UNDEF::INT)
          {
            changeTries += abs(newSetTemp - temp);
            changeTries += changeTries/10;
          }

          setTemp = newSetTemp;
          if (temp > setTemp)
          {
            getActualSetpoint = changeWaterTemp(+1);
            changeTries--;
          }
          else if (temp < setTemp)
          {
            getActualSetpoint = changeWaterTemp(-1);
            changeTries--;
          }
        }
      } while (temp != setTemp && changeTries);
    }
  }
}

void PureSpaIO::setDisinfectionTime(int hours)
{
  if (hours > 5)      hours = 8;
  else if (hours > 3) hours = 5;
  else if (hours > 0) hours = 3;
  else                hours = 0;

  if (isPowerOn() && state.error == 0)
  {
    int tries = 8;
    do
    {
      int actHours = getDisinfectionTime();
      if (actHours == UNDEF::INT)
      {
        break;
      }
      else if (actHours == hours)
      {
        break;
      }

      pressButton(buttons.toggleDisinfection);
      tries--;
    } while (tries);
  }
}

bool PureSpaIO::pressButton(volatile unsigned int& buttonPressCount)
{
  waitBuzzerOff();
  unsigned int tries = BUTTON::ACK_TIMEOUT/BUTTON::ACK_CHECK_PERIOD;
  buttonPressCount = BUTTON::PRESS_COUNT;
  while (buttonPressCount && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    tries--;
  }
  bool success = state.buzzer;

  return success;
}

void PureSpaIO::setBubbleOn(bool on)
{
  if (on ^ (isBubbleOn() == true))
  {
    pressButton(buttons.toggleBubble);
  }
}

void PureSpaIO::setFilterOn(bool on)
{
  if (on ^ (isFilterOn() == true))
  {
    pressButton(buttons.toggleFilter);
  }
}

void PureSpaIO::setHeaterOn(bool on)
{
  if (on ^ (isHeaterOn() == true || isHeaterStandby() == true))
  {
    pressButton(buttons.toggleHeater);
  }
}

void PureSpaIO::setJetOn(bool on)
{
  if (on ^ (isJetOn() == true))
  {
    pressButton(buttons.toggleJet);
  }
}

void PureSpaIO::setPowerOn(bool on)
{
  bool active = isPowerOn() == true;
  if (on ^ active)
  {
    pressButton(buttons.togglePower);
  }
}

bool PureSpaIO::waitBuzzerOff() const
{
  int tries = BUTTON::ACK_TIMEOUT/BUTTON::ACK_CHECK_PERIOD;
  while (state.buzzer && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    tries--;
  }

  if (tries)
  {
    delay(2*CYCLE::PERIOD);
    return true;
  }
  else
  {
    return false;
  }
}

bool PureSpaIO::changeWaterTemp(int up)
{
  bool success = false;

  if (isPowerOn() && state.error == 0)
  {
    waitBuzzerOff();

    int tries = BUTTON::PRESS_SHORT_COUNT*CYCLE::PERIOD/BUTTON::ACK_CHECK_PERIOD;
    if (up > 0)
    {
      buttons.toggleTempUp = BUTTON::PRESS_SHORT_COUNT;
      while (buttons.toggleTempUp && tries)
      {
        delay(BUTTON::ACK_CHECK_PERIOD);
        tries--;
      }
      buttons.toggleTempUp = 0;
    }
    else if (up < 0)
    {
      buttons.toggleTempDown = BUTTON::PRESS_SHORT_COUNT;
      while (buttons.toggleTempDown && tries)
      {
        delay(BUTTON::ACK_CHECK_PERIOD);
        tries--;
      }
      buttons.toggleTempDown = 0;
    }

    tries = (BUTTON::PRESS_COUNT - BUTTON::PRESS_SHORT_COUNT)*CYCLE::PERIOD/BUTTON::ACK_CHECK_PERIOD;
    while (!state.buzzer && tries)
    {
      delay(BUTTON::ACK_CHECK_PERIOD);
      tries--;
    }

    success = state.buzzer;
  }

  return success;
}

int PureSpaIO::convertDisplayToCelsius(uint32_t value) const
{
  int celsiusValue = display2Num(value);
  char tempUnit = display2LastDigit(value);
  if (tempUnit == 'F')
  {
    float fValue = (float)celsiusValue;
    celsiusValue = (int)round(((fValue - 32) * 5) / 9);
  }
  else if (tempUnit != 'C')
  {
    celsiusValue = UNDEF::INT;
  }

  return (celsiusValue >= 0) && (celsiusValue <= 60) ? celsiusValue : UNDEF::INT;
}

/*IRAM_ATTR void PureSpaIO::clockRisingISR(void* arg)
{
  isrState.frameValue = (isrState.frameValue << 1) + !gpio_get_level(PIN::DATA);
  isrState.receivedBits = isrState.receivedBits + 1;
}*/

IRAM_ATTR void PureSpaIO::clockRisingISR(void* arg)
{
  bool data = !gpio_get_level(PIN::DATA);
  bool enabled = !gpio_get_level(PIN::LATCH);

  if (enabled || isrState.receivedBits == (FRAME::BITS - 1))
  {
    isrState.frameValue = (isrState.frameValue << 1) + data;
    isrState.receivedBits = isrState.receivedBits + 1;

    if (isrState.receivedBits == FRAME::BITS)
    {
      state.frameCounter = state.frameCounter + 1;
      if (isrState.frameValue == FRAME_TYPE::CUE)
      {
      }
      else if (isrState.frameValue & FRAME_TYPE::DIGIT)
      {
        decodeDisplay();
      }
      else if (isrState.frameValue & FRAME_TYPE::LED)
      {
        decodeLED();
      }
      else if (isrState.frameValue & FRAME_TYPE::BUTTON)
      {
        decodeButton();
      }
      else if (isrState.frameValue != 0)
      {
      }

      isrState.receivedBits = 0;
    }
  }
  else
  {
    isrState.receivedBits = 0;
    state.frameDropped = state.frameDropped + 1;
    state.frameCounter = state.frameCounter + 1;
  }
}

IRAM_ATTR void PureSpaIO::latchRisingISR(void* arg)
{
  if (isrState.receivedBits == FRAME::BITS)
  {
    state.frameCounter = state.frameCounter + 1;
    
    if (isrState.frameValue == FRAME_TYPE::CUE) {
    }
    else if (isrState.frameValue & FRAME_TYPE::DIGIT) {
        decodeDisplay();
    }
    else if (isrState.frameValue & FRAME_TYPE::LED) {
        decodeLED();
    }
    else if (isrState.frameValue & FRAME_TYPE::BUTTON) {
        decodeButton();
    }

    isrState.receivedBits = 0;
  }
  else
  {
    state.frameCounter = state.frameCounter + 1;
    state.frameDropped = state.frameDropped + 1;
    isrState.receivedBits = 0;
  }
}

IRAM_ATTR void PureSpaIO::decodeDisplay()
{
  char digit;
  switch (isrState.frameValue & FRAME_DIGIT::SEGMENTS)
  {
    case FRAME_DIGIT::OFF:
      digit = DIGIT::OFF;
      break;
    case FRAME_DIGIT::NUM_0:
      digit = '0';
      break;
    case FRAME_DIGIT::NUM_1:
      digit = '1';
      break;
    case FRAME_DIGIT::NUM_2:
      digit = '2';
      break;
    case FRAME_DIGIT::NUM_3:
      digit = '3';
      break;
    case FRAME_DIGIT::NUM_4:
      digit = '4';
      break;
    case FRAME_DIGIT::NUM_5:
      digit = '5';
      break;
    case FRAME_DIGIT::NUM_6:
      digit = '6';
      break;
    case FRAME_DIGIT::NUM_7:
      digit = '7';
      break;
    case FRAME_DIGIT::NUM_8:
      digit = '8';
      break;
    case FRAME_DIGIT::NUM_9:
      digit = '9';
      break;
    case FRAME_DIGIT::LET_C:
      digit = 'C';
      break;
    case FRAME_DIGIT::LET_D:
      digit = 'D';
      break;
    case FRAME_DIGIT::LET_E:
      digit = 'E';
      break;
    case FRAME_DIGIT::LET_F:
      digit = 'F';
      break;
    case FRAME_DIGIT::LET_H:
      digit = 'H';
      break;
    case FRAME_DIGIT::LET_N:
      digit = 'N';
      break;
    default:
      return;
  }

  switch (isrState.frameValue & FRAME_TYPE::DIGIT)
  {
    case FRAME_DIGIT::POS_1:
      isrState.displayValue = (isrState.displayValue & 0xFFFFFF00U) + digit;
      isrState.receivedDigits = DIGIT::POS_1;
      break;

    case FRAME_DIGIT::POS_2:
      if (isrState.receivedDigits == DIGIT::POS_1)
      {
        isrState.displayValue = (isrState.displayValue & 0xFFFF00FFU) + (digit << 8);
        isrState.receivedDigits |= DIGIT::POS_2;
      }
      break;

    case FRAME_DIGIT::POS_3:
      if (isrState.receivedDigits == DIGIT::POS_1_2)
      {
        isrState.displayValue = (isrState.displayValue & 0xFF00FFFFU) + (digit << 16);
        isrState.receivedDigits |= DIGIT::POS_3;
      }
      break;

    case FRAME_DIGIT::POS_4:
      if (isrState.receivedDigits == DIGIT::POS_1_2_3)
      {
        isrState.displayValue = (isrState.displayValue & 0x00FFFFFFU) + (digit << 24);
        isrState.receivedDigits = DIGIT::POS_ALL;
      }
      break;
  }

  if (isrState.receivedDigits == DIGIT::POS_ALL)
  {
    if (isrState.displayValue == isrState.latestDisplayValue)
    {
      isrState.stableDisplayValueCount = isrState.stableDisplayValueCount - 1;
      if (isrState.stableDisplayValueCount == 0)
      {
        isrState.stableDisplayValueCount = CONFIRM_FRAMES::REGULAR;
        if (isrState.isDisplayBlinking)
        {
          if (diff(state.frameCounter, isrState.lastBlankDisplayFrameCounter) > BLINK::STOPPED_FRAMES)
          {
            isrState.isDisplayBlinking = false;
            isrState.latestBlinkingTemp = UNDEF::UINT;
          }
        }

        if (!displayIsError(isrState.displayValue))
        {
          if (displayIsTemp(isrState.displayValue))
          {
            if (isrState.isDisplayBlinking)
            {
              if (isrState.displayValue == isrState.latestBlinkingTemp)
              {
                isrState.stableBlinkingWaterTempCount = isrState.stableBlinkingWaterTempCount + 1;
              }
              else if (diff(state.frameCounter, isrState.lastBlankDisplayFrameCounter) < BLINK::TEMP_FRAMES)
              {
                isrState.latestBlinkingTemp = isrState.displayValue;
                isrState.stableBlinkingWaterTempCount = 0;
              }
            }
            else
            {
              if (isrState.displayValue == isrState.latestWaterTemp)
              {
                isrState.stableWaterTempCount = isrState.stableWaterTempCount - 1;
                if (isrState.stableWaterTempCount == 0)
                {
                  if (state.waterTemp != isrState.displayValue)
                  {
                    state.waterTemp = isrState.displayValue;
                  }

                  isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;
                }
              }
              else
              {
                isrState.latestWaterTemp = isrState.displayValue;
                isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;
              }
            }
          }
        }
        else
        {
          state.error = display2Error(isrState.displayValue);
        }
      }
    }
    else if (displayIsBlank(isrState.displayValue))
    {
      if (isrState.stableDisplayBlankCount)
      {
        isrState.stableDisplayBlankCount = isrState.stableDisplayBlankCount - 1;
      }
      else
      {
        if (isrState.isDisplayBlinking)
        {
          if (isrState.latestBlinkingTemp != UNDEF::UINT)
          {
            isrState.blankCounter = isrState.blankCounter + 1;
          }

          if (state.error == 0 && isrState.blankCounter > 2
              && isrState.stableBlinkingWaterTempCount >= CONFIRM_FRAMES::REGULAR
              && state.desiredTemp != isrState.latestBlinkingTemp)
          {
            state.desiredTemp = isrState.latestBlinkingTemp;
          }

          isrState.latestBlinkingTemp = UNDEF::UINT;
          isrState.stableBlinkingWaterTempCount = 0;
        }
        else
        {
          isrState.isDisplayBlinking = true;
          isrState.blankCounter = 0;
        }
        isrState.lastBlankDisplayFrameCounter = state.frameCounter;
      }
    }
    else
    {
      isrState.latestDisplayValue = isrState.displayValue;
      isrState.stableDisplayValueCount = CONFIRM_FRAMES::REGULAR;
      isrState.stableDisplayBlankCount = CONFIRM_FRAMES::REGULAR;
    }
  }
}

IRAM_ATTR void PureSpaIO::decodeLED()
{
  if (isrState.frameValue == isrState.latestLedStatus)
  {
    isrState.stableLedStatusCount = isrState.stableLedStatusCount - 1;
    if (isrState.stableLedStatusCount == 0)
    {
      state.ledStatus = isrState.frameValue;
      state.buzzer = !(state.ledStatus & FRAME_LED::NO_BEEP);
      state.stateUpdated = true;
      isrState.stableLedStatusCount = CONFIRM_FRAMES::REGULAR;

      if (state.buzzer)
      {
        buttons.toggleBubble = 0;
        buttons.toggleDisinfection = 0;
        buttons.toggleFilter = 0;
        buttons.toggleHeater = 0;
        buttons.toggleJet = 0;
        buttons.togglePower = 0;
        buttons.toggleTempUp = 0;
        buttons.toggleTempDown = 0;
      }
    }
  }
  else
  {
    isrState.latestLedStatus = isrState.frameValue;
    isrState.stableLedStatusCount = CONFIRM_FRAMES::REGULAR;
  }
}

IRAM_ATTR void PureSpaIO::updateButtonState(volatile unsigned int& buttonPressCount)
{
  if (buttonPressCount)
  {
    if (state.buzzer)
    {
      buttonPressCount = 0;
    }
    else
    {
      isrState.reply = true;
      buttonPressCount = buttonPressCount - 1;
    }
  }
}

IRAM_ATTR void PureSpaIO::decodeButton()
{
  if (isrState.frameValue & FRAME_BUTTON::FILTER)
  {
    updateButtonState(buttons.toggleFilter);
  }
  else if (isrState.frameValue & FRAME_BUTTON::HEATER)
  {
    updateButtonState(buttons.toggleHeater);
  }
  else if (isrState.frameValue & FRAME_BUTTON::BUBBLE)
  {
    updateButtonState(buttons.toggleBubble);
  }
  else if (isrState.frameValue & FRAME_BUTTON::POWER)
  {
    updateButtonState(buttons.togglePower);
  }
  else if (isrState.frameValue & FRAME_BUTTON::TEMP_UP)
  {
    updateButtonState(buttons.toggleTempUp);
  }
  else if (isrState.frameValue & FRAME_BUTTON::TEMP_DOWN)
  {
    updateButtonState(buttons.toggleTempDown);
  }

  if (isrState.reply)
  {
    delayMicroseconds(1);
    gpio_set_level(PIN::DATA, 0); // Explicitly pull low
    gpio_set_direction(PIN::DATA, GPIO_MODE_OUTPUT);
    delayMicroseconds(2);
    gpio_set_direction(PIN::DATA, GPIO_MODE_INPUT);
    isrState.reply = false;
  }
}
