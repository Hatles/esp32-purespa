#include <esp_log.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_sntp.h"

#include "wifi_manager.h"
#include "dns_server.h"
#include "captive_portal.h"
#include "web_server.h"
#include "PureSpaService.h"

static const char *TAG = "app_main";

void init_sntp() {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    
    // Set timezone to CET/CEST (example: Europe/Paris)
    // You might want to make this configurable
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Stopping webserver");
    WebServer::getInstance().stop();
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Starting webserver");
    WebServer::getInstance().start();
    init_sntp();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP Started, starting captive portal");
        DNSServer::getInstance().start();
        CaptivePortal::getInstance().start();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "AP Stopped, stopping captive portal");
        DNSServer::getInstance().stop();
        CaptivePortal::getInstance().stop();
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    WiFiManager::getInstance().init();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    PureSpaService::getInstance().init();
    WiFiManager::getInstance().startSTA();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
