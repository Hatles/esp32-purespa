#include <esp_log.h>
#include <esp_http_server.h>
#include <ctype.h>
#include "captive_portal.h"
#include "wifi_manager.h"
#include "esp_system.h"

static const char *TAG = "captive_portal";

static void url_decode(char *dst, const char *src) {
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

static esp_err_t config_get_handler(httpd_req_t *req) {
    const char* config_page = 
        "<!DOCTYPE html><html><head><title>ESP32 Wi-Fi Config</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body { font-family: Arial; margin: 20px; } input { width: 100%; padding: 10px; margin: 5px 0; } button { width: 100%; padding: 10px; background-color: #4CAF50; color: white; border: none; }</style>"
        "</head><body>"
        "<h1>Wi-Fi Configuration</h1>"
        "<form method='post' action='/config'>"
        "<label>SSID:</label><input type='text' name='ssid'>"
        "<label>Password:</label><input type='password' name='password'>"
        "<button type='submit'>Save & Restart</button>"
        "</form>"
        "</body></html>";
    httpd_resp_send(req, config_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t *req) {
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

    // Simple parser for application/x-www-form-urlencoded
    char *p = strtok(buf, "&");
    while (p != NULL) {
        if (strncmp(p, "ssid=", 5) == 0) {
            strncpy(ssid, p + 5, sizeof(ssid) - 1);
            url_decode(decoded_ssid, ssid);
        } else if (strncmp(p, "password=", 9) == 0) {
            strncpy(pass, p + 9, sizeof(pass) - 1);
            url_decode(decoded_pass, pass);
        }
        p = strtok(NULL, "&");
    }

    ESP_LOGI(TAG, "Received SSID: %s", decoded_ssid);
    wifi_manager_save_credentials(decoded_ssid, decoded_pass);

    httpd_resp_send(req, "Credentials saved. Restarting...", HTTPD_RESP_USE_STRLEN);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

// Redirect all 404 to index for captive portal
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/config");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t config_get = {
    .uri       = "/config",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t config_post = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_post_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_captive_portal(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.ctrl_port = 32769; // Use a different port for control to avoid conflict if another server is running

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &config_get);
        httpd_register_uri_handler(server, &config_post);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        return server;
    }
    return NULL;
}

void stop_captive_portal(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
    }
}
