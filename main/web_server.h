#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_http_server.h>

class WebServer {
public:
    static WebServer& getInstance() {
        static WebServer instance;
        return instance;
    }

    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    void start();
    void stop();

private:
    WebServer() : _mainServer(NULL), _sseServer(NULL) {}
    httpd_handle_t _mainServer;
    httpd_handle_t _sseServer;

    static esp_err_t rootGetHandler(httpd_req_t *req);
    static esp_err_t apiStatusHandler(httpd_req_t *req);
    static esp_err_t apiControlHandler(httpd_req_t *req);
    static esp_err_t apiSseHandler(httpd_req_t *req);
};

#endif // WEB_SERVER_H
