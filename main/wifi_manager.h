#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <esp_err.h>

void wifi_manager_init(void);
bool wifi_manager_is_connected(void);
void wifi_manager_start_ap(void);
void wifi_manager_start_sta(void);
void wifi_manager_save_credentials(const char *ssid, const char *password);

#endif // WIFI_MANAGER_H
