#ifndef PURE_SPA_SERVICE_H
#define PURE_SPA_SERVICE_H

#include "PureSpaIO.h"
#include <string>
#include <mutex>
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

private:
    PureSpaService() : _io(), _cmdQueue(nullptr) {}
    
    PureSpaIO _io;
    QueueHandle_t _cmdQueue;
    
    static void taskWrapper(void* param);
    void run();
    void sendRequest(SpaCommand cmd, int value = 0);
};

#endif // PURE_SPA_SERVICE_H
