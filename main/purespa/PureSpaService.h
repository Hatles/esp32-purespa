#ifndef PURE_SPA_SERVICE_H
#define PURE_SPA_SERVICE_H

#include "PureSpaIO.h"
#include <string>
#include <mutex>

class PureSpaService {
public:
    static PureSpaService& getInstance() {
        static PureSpaService instance;
        return instance;
    }

    // Delete copy constructor and assignment operator for Singleton
    PureSpaService(const PureSpaService&) = delete;
    PureSpaService& operator=(const PureSpaService&) = delete;

    void init();
    std::string getStatusJson();
    
    // Control methods
    void setPower(bool on);
    void setFilter(bool on);
    void setBubble(bool on);
    void setHeater(bool on);
    void setTargetTemp(int temp);

private:
    PureSpaService() : _io() {}
    
    PureSpaIO _io;
    static void taskWrapper(void* param);
    void run();
};

#endif // PURE_SPA_SERVICE_H
