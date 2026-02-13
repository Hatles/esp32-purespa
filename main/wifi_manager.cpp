#include "wifi_manager.h"
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "WiFiManager";

#define WIFI_SSID_KEY "wifi_ssid"
#define WIFI_PASS_KEY "wifi_pass"
#define WIFI_NAMESPACE "wifi_creds"

void WiFiManager::init() {
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WiFiManager::eventHandler,
                                                        this,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WiFiManager::eventHandler,
                                                        this,
                                                        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

void WiFiManager::eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiManager* self = static_cast<WiFiManager*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (self->_retryNum < MAX_RETRY) {
            esp_wifi_connect();
            self->_retryNum++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", self->_retryNum, MAX_RETRY);
        } else {
            ESP_LOGI(TAG, "Connect to the AP fail, starting AP mode");
            self->startAP();
        }
        self->_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        self->_retryNum = 0;
        self->_connected = true;
    }
}

void WiFiManager::startSTA() {
    nvs_handle_t nvs_handle;
    char ssid[32] = {0};
    char pass[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, WIFI_SSID_KEY, ssid, &ssid_len);
        if (err == ESP_OK) {
            nvs_get_str(nvs_handle, WIFI_PASS_KEY, pass, &pass_len);
        }
        nvs_close(nvs_handle);
    }

    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Starting STA mode with SSID: %s", ssid);
        wifi_config_t wifi_config = {};
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

        esp_wifi_stop();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        ESP_LOGI(TAG, "No credentials found, starting AP mode");
        startAP();
    }
}

void WiFiManager::startAP() {
    ESP_LOGI(TAG, "Starting AP mode");
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "ESP32-PureSpa-Config");
    wifi_config.ap.ssid_len = strlen("ESP32-PureSpa-Config");
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiManager::saveCredentials(const std::string& ssid, const std::string& password) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, WIFI_SSID_KEY, ssid.c_str()));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, WIFI_PASS_KEY, password.c_str()));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}
