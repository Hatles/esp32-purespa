#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string>
#include "esp_event.h"

class WiFiManager {
public:
    static WiFiManager& getInstance() {
        static WiFiManager instance;
        return instance;
    }

    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;

    void init();
    void startSTA();
    void startAP();
    bool isConnected() const { return _connected; }
    void saveCredentials(const std::string& ssid, const std::string& password);

private:
    WiFiManager() : _connected(false), _retryNum(0) {}
    
    bool _connected;
    int _retryNum;
    static const int MAX_RETRY = 5;

    static void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};

#endif // WIFI_MANAGER_H
