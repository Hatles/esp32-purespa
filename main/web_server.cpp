#include "web_server.h"
#include <esp_log.h>
#include <time.h>
#include <sys/time.h>
#include <cstring>
#include "cJSON.h"
#include "PureSpaService.h"

static const char *TAG = "WebServer";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

void WebServer::start() {
    if (_server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    static const httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = rootGetHandler,
        .user_ctx  = NULL
    };

    static const httpd_uri_t index_html = {
        .uri       = "/index.html",
        .method    = HTTP_GET,
        .handler   = rootGetHandler,
        .user_ctx  = NULL
    };

    static const httpd_uri_t api_status = {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = apiStatusHandler,
        .user_ctx  = NULL
    };

    static const httpd_uri_t api_control = {
        .uri       = "/api/control",
        .method    = HTTP_POST,
        .handler   = apiControlHandler,
        .user_ctx  = NULL
    };

    static const httpd_uri_t api_sse = {
        .uri       = "/api/sse",
        .method    = HTTP_GET,
        .handler   = apiSseHandler,
        .user_ctx  = NULL
    };

    ESP_LOGI(TAG, "Starting web server on port: %d", config.server_port);
    if (httpd_start(&_server, &config) == ESP_OK) {
        httpd_register_uri_handler(_server, &root);
        httpd_register_uri_handler(_server, &index_html);
        httpd_register_uri_handler(_server, &api_status);
        httpd_register_uri_handler(_server, &api_control);
        httpd_register_uri_handler(_server, &api_sse);
    }
}

void WebServer::stop() {
    if (_server != NULL) {
        httpd_stop(_server);
        _server = NULL;
    }
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
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

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

    char sse_data[300];
    
    while (1) {
        std::string status = PureSpaService::getInstance().getStatusJson();
        int len = snprintf(sse_data, sizeof(sse_data), "data: %s\n\n", status.c_str());
        if (httpd_resp_send_chunk(req, sse_data, len) != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
