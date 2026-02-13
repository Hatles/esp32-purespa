#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <esp_http_server.h>

httpd_handle_t start_captive_portal(void);
void stop_captive_portal(httpd_handle_t server);

#endif // CAPTIVE_PORTAL_H
