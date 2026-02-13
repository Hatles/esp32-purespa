#include "dns_server.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "esp_log.h"

static const char *TAG = "DNSServer";

#define DNS_PORT 53

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

void DNSServer::start() {
    if (_taskHandle == nullptr) {
        xTaskCreate(taskWrapper, "dns_server", 4096, this, 5, &_taskHandle);
    }
}

void DNSServer::stop() {
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
}

void DNSServer::taskWrapper(void* param) {
    static_cast<DNSServer*>(param)->run();
}

void DNSServer::run() {
    uint8_t rx_buffer[512];
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS server started on port %d", DNS_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        if (len < sizeof(dns_header_t)) continue;

        dns_header_t *header = (dns_header_t *)rx_buffer;
        header->flags = htons(0x8180);
        header->an_count = header->qd_count;

        uint8_t *ptr = rx_buffer + len;
        
        *ptr++ = 0xc0;
        *ptr++ = 0x0c;
        *ptr++ = 0x00;
        *ptr++ = 0x01;
        *ptr++ = 0x00;
        *ptr++ = 0x01;
        *ptr++ = 0x00;
        *ptr++ = 0x00;
        *ptr++ = 0x00;
        *ptr++ = 0x3c;
        *ptr++ = 0x00;
        *ptr++ = 0x04;
        *ptr++ = 192;
        *ptr++ = 168;
        *ptr++ = 4;
        *ptr++ = 1;

        sendto(sock, rx_buffer, ptr - rx_buffer, 0, (struct sockaddr *)&source_addr, socklen);
    }

    close(sock);
    _taskHandle = nullptr;
    vTaskDelete(NULL);
}
