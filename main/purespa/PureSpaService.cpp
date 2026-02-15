#include "PureSpaService.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "PureSpaService";

void PureSpaService::init() {
    _cmdQueue = xQueueCreate(10, sizeof(SpaRequest));
    if (_cmdQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue!");
        return;
    }

    ESP_LOGI(TAG, "Command queue created. Starting service task...");
    xTaskCreatePinnedToCore(taskWrapper, "purespa_service_task", 4096, this, 5, NULL, 1);
}

void PureSpaService::taskWrapper(void* param) {
    static_cast<PureSpaService*>(param)->run();
}

void PureSpaService::run() {
    ESP_LOGI(TAG, "PureSpa Service task running on core %d", xPortGetCoreID());
    _io.setup(LANG::EN);
    
    SpaRequest req;
    unsigned long lastReport = 0;
    
    while (true) {
        unsigned long now = (unsigned long)(esp_timer_get_time() / 1000);

        if (now - lastReport > 1000) {
            lastReport = now;
            ESP_LOGI(TAG, "Status: %s", getStatusJson().c_str());
        }

        // Check for new commands (non-blocking)
        if (xQueueReceive(_cmdQueue, &req, 0) == pdPASS) {
            ESP_LOGI(TAG, "Command received from queue: %d (val: %d)", (int)req.cmd, req.value);
            switch (req.cmd) {
                case SpaCommand::POWER_ON:   _io.setPowerOn(true); break;
                case SpaCommand::POWER_OFF:  _io.setPowerOn(false); break;
                case SpaCommand::FILTER_ON:  _io.setFilterOn(true); break;
                case SpaCommand::FILTER_OFF: _io.setFilterOn(false); break;
                case SpaCommand::BUBBLE_ON:  _io.setBubbleOn(true); break;
                case SpaCommand::BUBBLE_OFF: _io.setBubbleOn(false); break;
                case SpaCommand::HEATER_ON:  _io.setHeaterOn(true); break;
                case SpaCommand::HEATER_OFF: _io.setHeaterOn(false); break;
                case SpaCommand::SET_TEMP:   _io.setDesiredWaterTempCelsius(req.value); break;
                default: ESP_LOGW(TAG, "Unknown command type: %d", (int)req.cmd); break;
            }
            ESP_LOGI(TAG, "Command %d execution finished.", (int)req.cmd);
        }

        _io.loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

std::string PureSpaService::getStatusJson() {
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(root, "online", _io.isOnline());
    cJSON_AddNumberToObject(root, "act_temp", _io.getActWaterTempCelsius());
    cJSON_AddNumberToObject(root, "set_temp", _io.getDesiredWaterTempCelsius());
    cJSON_AddBoolToObject(root, "power", _io.isPowerOn());
    cJSON_AddBoolToObject(root, "filter", _io.isFilterOn());
    cJSON_AddBoolToObject(root, "heater", _io.isHeaterOn());
    cJSON_AddBoolToObject(root, "bubble", _io.isBubbleOn());
    
    char *json_str = cJSON_PrintUnformatted(root);
    std::string result = json_str ? json_str : "{}";
    
    if (json_str) free(json_str);
    cJSON_Delete(root);
    
    return result;
}

void PureSpaService::sendRequest(SpaCommand cmd, int value) {
    if (_cmdQueue == nullptr) {
        ESP_LOGE(TAG, "Cannot send request: queue is NULL");
        return;
    }
    SpaRequest req = {cmd, value};
    ESP_LOGI(TAG, "Sending request %d to queue...", (int)cmd);
    if (xQueueSend(_cmdQueue, &req, pdMS_TO_TICKS(10)) != pdPASS) {
        ESP_LOGW(TAG, "Queue is FULL, could not send request %d", (int)cmd);
    } else {
        ESP_LOGI(TAG, "Request %d queued successfully.", (int)cmd);
    }
}

void PureSpaService::setPower(bool on) {
    sendRequest(on ? SpaCommand::POWER_ON : SpaCommand::POWER_OFF);
}

void PureSpaService::setFilter(bool on) {
    sendRequest(on ? SpaCommand::FILTER_ON : SpaCommand::FILTER_OFF);
}

void PureSpaService::setBubble(bool on) {
    sendRequest(on ? SpaCommand::BUBBLE_ON : SpaCommand::BUBBLE_OFF);
}

void PureSpaService::setHeater(bool on) {
    sendRequest(on ? SpaCommand::HEATER_ON : SpaCommand::HEATER_OFF);
}

void PureSpaService::setTargetTemp(int temp) {
    sendRequest(SpaCommand::SET_TEMP, temp);
}
