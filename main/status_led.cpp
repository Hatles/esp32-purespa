#include "status_led.h"
#include <esp_log.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "StatusLed";
#define STATUS_LED_PIN GPIO_NUM_2
#define LED_ON  1
#define LED_OFF 0

void StatusLed::init() {
    ESP_LOGI(TAG, "Initializing Status LED on GPIO %d", STATUS_LED_PIN);
    gpio_reset_pin(STATUS_LED_PIN);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, LED_OFF);

    xTaskCreate(ledTask, "status_led_task", 2048, this, 2, NULL);
}

void StatusLed::setState(LedState state) {
    _state = state;
}

void StatusLed::ledTask(void* param) {
    StatusLed* self = static_cast<StatusLed*>(param);
    
    while (true) {
        switch (self->_state) {
            case LedState::OFF:
                gpio_set_level(STATUS_LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case LedState::AP_MODE: // Slow blink: 1s ON, 1s OFF
                gpio_set_level(STATUS_LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(1000));
                gpio_set_level(STATUS_LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case LedState::CONNECTING: // Fast blink: 200ms ON, 200ms OFF
                gpio_set_level(STATUS_LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(STATUS_LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
                
            case LedState::CONNECTED: // Solid ON
                gpio_set_level(STATUS_LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case LedState::ERROR: // Double blink: 150ms ON/OFF x2, 1s pause
                gpio_set_level(STATUS_LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(150));
                gpio_set_level(STATUS_LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(150));
                gpio_set_level(STATUS_LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(150));
                gpio_set_level(STATUS_LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}
