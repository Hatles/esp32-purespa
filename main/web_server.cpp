#include "web_server.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <time.h>
#include <sys/time.h>
#include <cstring>
#include "cJSON.h"
#include "PureSpaService.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "WebServer";

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[]   asm("_binary_index_html_gz_end");
extern const uint8_t favicon_png_start[] asm("_binary_favicon_png_start");
extern const uint8_t favicon_png_end[]   asm("_binary_favicon_png_end");

void WebServer::start() {
    if (_mainServer != NULL) return;

    // 1. MAIN SERVER (Port 80)
    httpd_config_t configMain = HTTPD_DEFAULT_CONFIG();
    configMain.server_port = 80;
    configMain.lru_purge_enable = true;
    configMain.max_uri_handlers = 20;

    static const httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = rootGetHandler, .user_ctx = NULL };
    static const httpd_uri_t index_html = { .uri = "/index.html", .method = HTTP_GET, .handler = rootGetHandler, .user_ctx = NULL };
    static const httpd_uri_t favicon_ico = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = faviconGetHandler, .user_ctx = NULL };
    static const httpd_uri_t api_status = { .uri = "/api/status", .method = HTTP_GET, .handler = apiStatusHandler, .user_ctx = NULL };
    static const httpd_uri_t api_control = { .uri = "/api/control", .method = HTTP_POST, .handler = apiControlHandler, .user_ctx = NULL };
    static const httpd_uri_t api_schedule_get = { .uri = "/api/schedule", .method = HTTP_GET, .handler = apiScheduleGetHandler, .user_ctx = NULL };
    static const httpd_uri_t api_schedule_add = { .uri = "/api/schedule/add", .method = HTTP_POST, .handler = apiScheduleAddHandler, .user_ctx = NULL };
    static const httpd_uri_t api_schedule_update = { .uri = "/api/schedule/update", .method = HTTP_POST, .handler = apiScheduleUpdateHandler, .user_ctx = NULL };
    static const httpd_uri_t api_schedule_delete = { .uri = "/api/schedule/delete", .method = HTTP_POST, .handler = apiScheduleDeleteHandler, .user_ctx = NULL };
    static const httpd_uri_t api_schedule_toggle = { .uri = "/api/schedule/toggle", .method = HTTP_POST, .handler = apiScheduleToggleHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_time = { .uri = "/api/admin/time", .method = HTTP_POST, .handler = apiAdminTimeHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_reboot = { .uri = "/api/admin/reboot", .method = HTTP_POST, .handler = apiAdminRebootHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_reset_wifi = { .uri = "/api/admin/reset/wifi", .method = HTTP_POST, .handler = apiAdminResetWifiHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_reset_schedule = { .uri = "/api/admin/reset/schedule", .method = HTTP_POST, .handler = apiAdminResetScheduleHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_reset_all = { .uri = "/api/admin/reset/all", .method = HTTP_POST, .handler = apiAdminResetAllHandler, .user_ctx = NULL };
    static const httpd_uri_t api_admin_ota = { .uri = "/api/admin/ota", .method = HTTP_POST, .handler = apiAdminOtaHandler, .user_ctx = NULL };

    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "Starting Main server on port 80. Visit http://" IPSTR "/", IP2STR(&ip_info.ip));
    } else {
        ESP_LOGI(TAG, "Starting Main server on port 80. Visit http://192.168.4.1/ (AP Mode)");
    }

    if (httpd_start(&_mainServer, &configMain) == ESP_OK) {
        httpd_register_uri_handler(_mainServer, &root);
        httpd_register_uri_handler(_mainServer, &index_html);
        httpd_register_uri_handler(_mainServer, &favicon_ico);
        httpd_register_uri_handler(_mainServer, &api_status);
        httpd_register_uri_handler(_mainServer, &api_control);
        httpd_register_uri_handler(_mainServer, &api_schedule_get);
        httpd_register_uri_handler(_mainServer, &api_schedule_add);
        httpd_register_uri_handler(_mainServer, &api_schedule_update);
        httpd_register_uri_handler(_mainServer, &api_schedule_delete);
        httpd_register_uri_handler(_mainServer, &api_schedule_toggle);
        httpd_register_uri_handler(_mainServer, &api_admin_time);
        httpd_register_uri_handler(_mainServer, &api_admin_reboot);
        httpd_register_uri_handler(_mainServer, &api_admin_reset_wifi);
        httpd_register_uri_handler(_mainServer, &api_admin_reset_schedule);
        httpd_register_uri_handler(_mainServer, &api_admin_reset_all);
        httpd_register_uri_handler(_mainServer, &api_admin_ota);
    }

    /*
    // 2. SSE SERVER (Port 81)
    httpd_config_t configSSE = HTTPD_DEFAULT_CONFIG();
    configSSE.server_port = 81;
    configSSE.ctrl_port = 32770; // Different control port
    configSSE.lru_purge_enable = true;

    static const httpd_uri_t api_sse = { .uri = "/api/sse", .method = HTTP_GET, .handler = apiSseHandler };

    ESP_LOGI(TAG, "Starting SSE server on port 81");
    if (httpd_start(&_sseServer, &configSSE) == ESP_OK) {
        httpd_register_uri_handler(_sseServer, &api_sse);
    }
    */
}

void WebServer::stop() {
    if (_mainServer) httpd_stop(_mainServer);
    if (_sseServer) httpd_stop(_sseServer);
    _mainServer = NULL;
    _sseServer = NULL;
}

esp_err_t WebServer::rootGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)index_html_gz_start, index_html_gz_end - index_html_gz_start);
    return ESP_OK;
}

esp_err_t WebServer::faviconGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char *)favicon_png_start, favicon_png_end - favicon_png_start);
    return ESP_OK;
}

esp_err_t WebServer::apiStatusHandler(httpd_req_t *req) {
    std::string status = PureSpaService::getInstance().getStatusJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, status.c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiControlHandler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    PureSpaService& service = PureSpaService::getInstance();
    cJSON *item = cJSON_GetObjectItem(json, "cmd");
    if (cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "power") == 0) {
            cJSON *val = cJSON_GetObjectItem(json, "value");
            if (cJSON_IsBool(val)) service.setPower(cJSON_IsTrue(val));
        } else if (strcmp(item->valuestring, "filter") == 0) {
            cJSON *val = cJSON_GetObjectItem(json, "value");
            if (cJSON_IsBool(val)) service.setFilter(cJSON_IsTrue(val));
        } else if (strcmp(item->valuestring, "bubble") == 0) {
            cJSON *val = cJSON_GetObjectItem(json, "value");
            if (cJSON_IsBool(val)) service.setBubble(cJSON_IsTrue(val));
        } else if (strcmp(item->valuestring, "heater") == 0) {
            cJSON *val = cJSON_GetObjectItem(json, "value");
            if (cJSON_IsBool(val)) service.setHeater(cJSON_IsTrue(val));
        } else if (strcmp(item->valuestring, "temp") == 0) {
            cJSON *val = cJSON_GetObjectItem(json, "value");
            if (cJSON_IsNumber(val)) service.setTargetTemp(val->valueint);
        }
    }
    cJSON_Delete(json);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiSseHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    // Allow Port 80 UI to connect to Port 81 SSE
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char sse_data[300];
    esp_err_t err = ESP_OK;
    
    while (err == ESP_OK) {
        std::string status = PureSpaService::getInstance().getStatusJson();
        int len = snprintf(sse_data, sizeof(sse_data), "data: %s\n\n", status.c_str());
        err = httpd_resp_send_chunk(req, sse_data, len);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::apiScheduleGetHandler(httpd_req_t *req) {
    std::string schedule = PureSpaService::getInstance().getScheduleJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, schedule.c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiScheduleAddHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/schedule/add");
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    ScheduledEvent ev;
    ev.enabled = true;
    ev.recurring = cJSON_IsTrue(cJSON_GetObjectItem(json, "recurring"));
    ev.dayOfWeekMask = cJSON_GetObjectItem(json, "dow") ? cJSON_GetObjectItem(json, "dow")->valueint : 0;
    ev.year = cJSON_GetObjectItem(json, "year") ? cJSON_GetObjectItem(json, "year")->valueint : 0;
    ev.month = cJSON_GetObjectItem(json, "month") ? cJSON_GetObjectItem(json, "month")->valueint : 0;
    ev.day = cJSON_GetObjectItem(json, "day") ? cJSON_GetObjectItem(json, "day")->valueint : 0;
    ev.hour = cJSON_GetObjectItem(json, "hour") ? cJSON_GetObjectItem(json, "hour")->valueint : 0;
    ev.minute = cJSON_GetObjectItem(json, "minute") ? cJSON_GetObjectItem(json, "minute")->valueint : 0;
    
    ev.setPower = cJSON_IsBool(cJSON_GetObjectItem(json, "setPower")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setPower")) : false;
    ev.powerValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "powerValue"));
    ev.setFilter = cJSON_IsBool(cJSON_GetObjectItem(json, "setFilter")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setFilter")) : false;
    ev.filterValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "filterValue"));
    ev.setHeater = cJSON_IsBool(cJSON_GetObjectItem(json, "setHeater")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setHeater")) : false;
    ev.heaterValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "heaterValue"));
    ev.setBubble = cJSON_IsBool(cJSON_GetObjectItem(json, "setBubble")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setBubble")) : false;
    ev.bubbleValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "bubbleValue"));
    ev.setTargetTemp = cJSON_IsBool(cJSON_GetObjectItem(json, "setTemp")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setTemp")) : false;
    ev.targetTempValue = cJSON_GetObjectItem(json, "tempValue") ? cJSON_GetObjectItem(json, "tempValue")->valueint : 38;

    PureSpaService::getInstance().addEvent(ev);
    
    cJSON_Delete(json);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiScheduleUpdateHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/schedule/update");
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    if (!cJSON_IsNumber(id_item)) {
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    int id = id_item->valueint;

    ScheduledEvent ev;
    ev.enabled = cJSON_IsBool(cJSON_GetObjectItem(json, "enabled")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled")) : true;
    ev.recurring = cJSON_IsTrue(cJSON_GetObjectItem(json, "recurring"));
    ev.dayOfWeekMask = cJSON_GetObjectItem(json, "dow") ? cJSON_GetObjectItem(json, "dow")->valueint : 0;
    ev.year = cJSON_GetObjectItem(json, "year") ? cJSON_GetObjectItem(json, "year")->valueint : 0;
    ev.month = cJSON_GetObjectItem(json, "month") ? cJSON_GetObjectItem(json, "month")->valueint : 0;
    ev.day = cJSON_GetObjectItem(json, "day") ? cJSON_GetObjectItem(json, "day")->valueint : 0;
    ev.hour = cJSON_GetObjectItem(json, "hour") ? cJSON_GetObjectItem(json, "hour")->valueint : 0;
    ev.minute = cJSON_GetObjectItem(json, "minute") ? cJSON_GetObjectItem(json, "minute")->valueint : 0;
    
    ev.setPower = cJSON_IsBool(cJSON_GetObjectItem(json, "setPower")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setPower")) : false;
    ev.powerValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "powerValue"));
    ev.setFilter = cJSON_IsBool(cJSON_GetObjectItem(json, "setFilter")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setFilter")) : false;
    ev.filterValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "filterValue"));
    ev.setHeater = cJSON_IsBool(cJSON_GetObjectItem(json, "setHeater")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setHeater")) : false;
    ev.heaterValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "heaterValue"));
    ev.setBubble = cJSON_IsBool(cJSON_GetObjectItem(json, "setBubble")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setBubble")) : false;
    ev.bubbleValue = cJSON_IsTrue(cJSON_GetObjectItem(json, "bubbleValue"));
    ev.setTargetTemp = cJSON_IsBool(cJSON_GetObjectItem(json, "setTemp")) ? cJSON_IsTrue(cJSON_GetObjectItem(json, "setTemp")) : false;
    ev.targetTempValue = cJSON_GetObjectItem(json, "tempValue") ? cJSON_GetObjectItem(json, "tempValue")->valueint : 38;

    PureSpaService::getInstance().updateEvent(id, ev);
    
    cJSON_Delete(json);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiScheduleDeleteHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/schedule/delete");
    char buf[64];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    int id = cJSON_GetObjectItem(json, "id")->valueint;
    PureSpaService::getInstance().deleteEvent(id);

    cJSON_Delete(json);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiScheduleToggleHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/schedule/toggle");
    char buf[64];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    int id = cJSON_GetObjectItem(json, "id")->valueint;
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    PureSpaService::getInstance().toggleEvent(id, enabled);

    cJSON_Delete(json);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiAdminTimeHandler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) return ESP_FAIL;

    cJSON *ts_item = cJSON_GetObjectItem(json, "timestamp");
    if (cJSON_IsNumber(ts_item)) {
        time_t timestamp = ts_item->valueint;
        struct timeval tv;
        tv.tv_sec = timestamp;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "System time synchronized to: %ld", (long)timestamp);
    }

    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void reboot_task(void *param) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

esp_err_t WebServer::apiAdminRebootHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Reboot requested");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t WebServer::apiAdminResetWifiHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Reset Wi-Fi credentials requested");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Wi-Fi credentials erased from NVS");
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t WebServer::apiAdminResetScheduleHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Reset schedule requested");
    PureSpaService::getInstance().clearSchedule();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::apiAdminResetAllHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Factory reset requested");
    
    nvs_flash_erase();
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t WebServer::apiAdminOtaHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Starting OTA update upload...");

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No passive OTA partition found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, (unsigned long)update_partition->address);

    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(1024);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for buffer");
        esp_ota_abort(update_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int received = 0;
    while (remaining > 0) {
        int read_len = (remaining > 1024) ? 1024 : remaining;
        int ret = httpd_req_recv(req, buf, read_len);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Connection closed or error during OTA receive (%d)", ret);
            free(buf);
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Connection lost during upload");
            return ESP_FAIL;
        }

        err = esp_ota_write(update_handle, (const void *)buf, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash write failed");
            return ESP_FAIL;
        }

        remaining -= ret;
        received += ret;
        if (received % 102400 == 0 || remaining == 0) {
            ESP_LOGI(TAG, "OTA Progress: %d / %d bytes written", received, (int)req->content_len);
        }
    }

    free(buf);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image validation failed");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation end failed");
        }
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set boot partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful! Rebooting in 1 second...");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);

    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}
