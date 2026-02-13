#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "esp_log.h"
#include "dns_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dns_server";

#define DNS_PORT 53

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t class;
} dns_question_t;

static void dns_server_task(void *pvParameters) {
    uint8_t rx_buffer[512];
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
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
        header->flags = htons(0x8180); // Standard query response, no error
        header->an_count = header->qd_count;

        uint8_t *ptr = rx_buffer + len;
        
        // Append Answer section
        // For simplicity, we just respond with 192.168.4.1 for every question
        // We need to mirror the question first
        // But the question is already in rx_buffer, we just append the answer after it
        
        // Answer section:
        // Name: 0xc00c (Pointer to the name in the question section)
        *ptr++ = 0xc0;
        *ptr++ = 0x0c;
        // Type: A (1)
        *ptr++ = 0x00;
        *ptr++ = 0x01;
        // Class: IN (1)
        *ptr++ = 0x00;
        *ptr++ = 0x01;
        // TTL: 60s
        *ptr++ = 0x00;
        *ptr++ = 0x00;
        *ptr++ = 0x00;
        *ptr++ = 0x3c;
        // Data length: 4
        *ptr++ = 0x00;
        *ptr++ = 0x04;
        // IP Address: 192.168.4.1
        *ptr++ = 192;
        *ptr++ = 168;
        *ptr++ = 4;
        *ptr++ = 1;

        sendto(sock, rx_buffer, ptr - rx_buffer, 0, (struct sockaddr *)&source_addr, socklen);
    }

    close(sock);
    vTaskDelete(NULL);
}

static TaskHandle_t dns_task_handle = NULL;

void dns_server_start(void) {
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
}

void dns_server_stop(void) {
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
}
