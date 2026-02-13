#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <esp_http_server.h>

class CaptivePortal {
public:
    static CaptivePortal& getInstance() {
        static CaptivePortal instance;
        return instance;
    }

    CaptivePortal(const CaptivePortal&) = delete;
    CaptivePortal& operator=(const CaptivePortal&) = delete;

    void start();
    void stop();

private:
    CaptivePortal() : _server(NULL) {}
    httpd_handle_t _server;

    static esp_err_t configGetHandler(httpd_req_t *req);
    static esp_err_t configPostHandler(httpd_req_t *req);
    static esp_err_t http404ErrorHandler(httpd_req_t *req, httpd_err_code_t err);
    static void urlDecode(char *dst, const char *src);
};

#endif // CAPTIVE_PORTAL_H
