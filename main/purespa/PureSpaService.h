#ifndef PURE_SPA_SERVICE_H
#define PURE_SPA_SERVICE_H

#include "PureSpaIO.h"
#include <string>
#include <mutex>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

enum class SpaCommand {
    NONE,
    POWER_ON, POWER_OFF,
    FILTER_ON, FILTER_OFF,
    BUBBLE_ON, BUBBLE_OFF,
    HEATER_ON, HEATER_OFF,
    SET_TEMP
};

struct SpaRequest {
    SpaCommand cmd;
    int value;
};

struct ScheduledEvent {
    int id;
    bool enabled;
    bool recurring; // true for day of week, false for specific date
    int dayOfWeekMask; // bits 0-6: Sun, Mon, Tue, Wed, Thu, Fri, Sat
    int year, month, day; // for non-recurring
    int hour, minute;
    
    // Actions (using optional-like pattern with boolean flags)
    bool setPower;
    bool powerValue;
    bool setFilter;
    bool filterValue;
    bool setHeater;
    bool heaterValue;
    bool setBubble;
    bool bubbleValue;
    bool setTargetTemp;
    int targetTempValue;
};

class PureSpaService {
public:
    static PureSpaService& getInstance() {
        static PureSpaService instance;
        return instance;
    }

    PureSpaService(const PureSpaService&) = delete;
    PureSpaService& operator=(const PureSpaService&) = delete;

    void init();
    std::string getStatusJson();
    
    void setPower(bool on);
    void setFilter(bool on);
    void setBubble(bool on);
    void setHeater(bool on);
    void setTargetTemp(int temp);

    // Scheduling
    std::string getScheduleJson();
    void addEvent(const ScheduledEvent& event);
    void deleteEvent(int id);
    void toggleEvent(int id, bool enabled);
    void loadSchedule();
    void saveSchedule();

private:
    PureSpaService() : _io(), _cmdQueue(nullptr), _nextEventId(1) {}
    
    PureSpaIO _io;
    QueueHandle_t _cmdQueue;
    
    std::vector<ScheduledEvent> _events;
    std::recursive_mutex _eventsMutex;
    int32_t _nextEventId;
    int _lastCheckedMinute = -1;

    static void taskWrapper(void* param);
    void run();
    void sendRequest(SpaCommand cmd, int value = 0);
    void checkSchedule();
    void executeEvent(const ScheduledEvent& event);
};

#endif // PURE_SPA_SERVICE_H
