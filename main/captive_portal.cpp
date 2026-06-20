#include "captive_portal.h"
#include <esp_log.h>
#include <ctype.h>
#include <cstring>
#include "wifi_manager.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CaptivePortal";

extern const uint8_t config_html_gz_start[] asm("_binary_config_html_gz_start");
extern const uint8_t config_html_gz_end[]   asm("_binary_config_html_gz_end");

void CaptivePortal::start() {
    if (_server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.ctrl_port = 32769;

    static const httpd_uri_t config_get = {
        .uri       = "/config",
        .method    = HTTP_GET,
        .handler   = configGetHandler,
        .user_ctx  = NULL
    };

    static const httpd_uri_t config_post = {
        .uri       = "/config",
        .method    = HTTP_POST,
        .handler   = configPostHandler,
        .user_ctx  = NULL
    };

    if (httpd_start(&_server, &config) == ESP_OK) {
        httpd_register_uri_handler(_server, &config_get);
        httpd_register_uri_handler(_server, &config_post);
        httpd_register_err_handler(_server, HTTPD_404_NOT_FOUND, http404ErrorHandler);
        ESP_LOGI(TAG, "Captive Portal started");
    }
}

void CaptivePortal::stop() {
    if (_server != NULL) {
        httpd_stop(_server);
        _server = NULL;
        ESP_LOGI(TAG, "Captive Portal stopped");
    }
}

void CaptivePortal::urlDecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

esp_err_t CaptivePortal::configGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)config_html_gz_start, config_html_gz_end - config_html_gz_start);
    return ESP_OK;
}

esp_err_t CaptivePortal::configPostHandler(httpd_req_t *req) {
    char buf[128];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};
    char decoded_ssid[32] = {0};
    char decoded_pass[64] = {0};

    char *p = strtok(buf, "&");
    while (p != NULL) {
        if (strncmp(p, "ssid=", 5) == 0) {
            strncpy(ssid, p + 5, sizeof(ssid) - 1);
            urlDecode(decoded_ssid, ssid);
        } else if (strncmp(p, "password=", 9) == 0) {
            strncpy(pass, p + 9, sizeof(pass) - 1);
            urlDecode(decoded_pass, pass);
        }
        p = strtok(NULL, "&");
    }

    ESP_LOGI(TAG, "Received SSID: %s", decoded_ssid);
    WiFiManager::getInstance().saveCredentials(decoded_ssid, decoded_pass);

    httpd_resp_send(req, "Credentials saved. Restarting...", HTTPD_RESP_USE_STRLEN);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t CaptivePortal::http404ErrorHandler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/config");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
