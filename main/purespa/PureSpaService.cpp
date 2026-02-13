#include "PureSpaService.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "PureSpaService";

void PureSpaService::init() {
    xTaskCreatePinnedToCore(taskWrapper, "purespa_service_task", 4096, this, 5, NULL, 1);
}

void PureSpaService::taskWrapper(void* param) {
    static_cast<PureSpaService*>(param)->run();
}

void PureSpaService::run() {
    ESP_LOGI(TAG, "PureSpa Service starting on core %d", xPortGetCoreID());
    _io.setup(LANG::EN);
    
    while (true) {
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

void PureSpaService::setPower(bool on) {
    _io.setPowerOn(on);
}

void PureSpaService::setFilter(bool on) {
    _io.setFilterOn(on);
}

void PureSpaService::setBubble(bool on) {
    _io.setBubbleOn(on);
}

void PureSpaService::setHeater(bool on) {
    _io.setHeaterOn(on);
}

void PureSpaService::setTargetTemp(int temp) {
    _io.setDesiredWaterTempCelsius(temp);
}
