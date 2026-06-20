#ifndef STATUS_LED_H
#define STATUS_LED_H

enum class LedState {
    OFF,
    AP_MODE,      // Slow blink (1s ON, 1s OFF)
    CONNECTING,   // Fast blink (200ms ON, 200ms OFF)
    CONNECTED,    // Solid ON
    ERROR         // Double blink (150ms ON/OFF x2, 1s pause)
};

class StatusLed {
public:
    static StatusLed& getInstance() {
        static StatusLed instance;
        return instance;
    }

    StatusLed(const StatusLed&) = delete;
    StatusLed& operator=(const StatusLed&) = delete;

    void init();
    void setState(LedState state);
    LedState getState() const { return _state; }

private:
    StatusLed() : _state(LedState::OFF) {}
    LedState _state;
    static void ledTask(void* param);
};

#endif // STATUS_LED_H
