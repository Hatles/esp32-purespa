#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DNSServer {
public:
    static DNSServer& getInstance() {
        static DNSServer instance;
        return instance;
    }

    DNSServer(const DNSServer&) = delete;
    DNSServer& operator=(const DNSServer&) = delete;

    void start();
    void stop();

private:
    DNSServer() : _taskHandle(nullptr) {}
    TaskHandle_t _taskHandle;
    static void taskWrapper(void* param);
    void run();
};

#endif // DNS_SERVER_H
