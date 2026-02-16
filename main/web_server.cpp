#include "web_server.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <time.h>
#include <sys/time.h>
#include <cstring>
#include "cJSON.h"
#include "PureSpaService.h"

static const char *TAG = "WebServer";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

void WebServer::start() {
    if (_mainServer != NULL) return;

    // 1. MAIN SERVER (Port 80)
    httpd_config_t configMain = HTTPD_DEFAULT_CONFIG();
    configMain.server_port = 80;
    configMain.lru_purge_enable = true;

    static const httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = rootGetHandler };
    static const httpd_uri_t index_html = { .uri = "/index.html", .method = HTTP_GET, .handler = rootGetHandler };
    static const httpd_uri_t api_status = { .uri = "/api/status", .method = HTTP_GET, .handler = apiStatusHandler };
    static const httpd_uri_t api_control = { .uri = "/api/control", .method = HTTP_POST, .handler = apiControlHandler };
    static const httpd_uri_t api_schedule_get = { .uri = "/api/schedule", .method = HTTP_GET, .handler = apiScheduleGetHandler };
    static const httpd_uri_t api_schedule_add = { .uri = "/api/schedule/add", .method = HTTP_POST, .handler = apiScheduleAddHandler };
    static const httpd_uri_t api_schedule_delete = { .uri = "/api/schedule/delete", .method = HTTP_POST, .handler = apiScheduleDeleteHandler };
    static const httpd_uri_t api_schedule_toggle = { .uri = "/api/schedule/toggle", .method = HTTP_POST, .handler = apiScheduleToggleHandler };

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
        httpd_register_uri_handler(_mainServer, &api_status);
        httpd_register_uri_handler(_mainServer, &api_control);
        httpd_register_uri_handler(_mainServer, &api_schedule_get);
        httpd_register_uri_handler(_mainServer, &api_schedule_add);
        httpd_register_uri_handler(_mainServer, &api_schedule_delete);
        httpd_register_uri_handler(_mainServer, &api_schedule_toggle);
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
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

esp_err_t WebServer::apiStatusHandler(httpd_req_t *req) {
    std::string status = PureSpaService::getInstance().getStatusJson();
    httpd_resp_set_type(req, "application/json");
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
