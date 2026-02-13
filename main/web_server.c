#include <esp_log.h>
#include <esp_http_server.h>
#include <time.h>
#include <sys/time.h>
#include "web_server.h"

static const char *TAG = "web_server";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/* Root and index.html handler */
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t index_html = {
    .uri       = "/index.html",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

/* /api/test endpoint */
static esp_err_t api_test_handler(httpd_req_t *req) {
    const char* resp = "{\"status\": \"ok\", \"message\": \"API is working\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t api_test = {
    .uri       = "/api/test",
    .method    = HTTP_GET,
    .handler   = api_test_handler,
    .user_ctx  = NULL
};

/* /api/sse endpoint */
static esp_err_t api_sse_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    char sse_data[64];
    while (1) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t now = tv.tv_sec;
        int len = snprintf(sse_data, sizeof(sse_data), "data: {\"time\": %lld}\n\n", now);
        if (httpd_resp_send_chunk(req, sse_data, len) != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t api_sse = {
    .uri       = "/api/sse",
    .method    = HTTP_GET,
    .handler   = api_sse_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting web server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &index_html);
        httpd_register_uri_handler(server, &api_test);
        httpd_register_uri_handler(server, &api_sse);
        return server;
    }
    return NULL;
}

esp_err_t stop_webserver(httpd_handle_t server) {
    if (server) {
        return httpd_stop(server);
    }
    return ESP_OK;
}
