#include "PureSpaService.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include <time.h>
#include <algorithm>

static const char *TAG = "PureSpaService";
#define SCHEDULE_NAMESPACE "purespa_sched"
#define SCHEDULE_KEY "events"

void PureSpaService::init() {
    _cmdQueue = xQueueCreate(10, sizeof(SpaRequest));
    if (_cmdQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue!");
        return;
    }

    loadSchedule();

    ESP_LOGI(TAG, "Command queue created. Starting service task...");
    xTaskCreatePinnedToCore(taskWrapper, "purespa_service_task", 8192, this, 5, NULL, 1);
}

void PureSpaService::taskWrapper(void* param) {
    static_cast<PureSpaService*>(param)->run();
}

void PureSpaService::run() {
    ESP_LOGI(TAG, "PureSpa Service task running on core %d", xPortGetCoreID());
    _io.setup(LANG::EN);
    
    SpaRequest req;
    TickType_t lastCheck = xTaskGetTickCount();
    
    while (true) {
        _io.loop();

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

        // Check schedule every 10 seconds (checkSchedule handles per-minute precision)
        if (xTaskGetTickCount() - lastCheck > pdMS_TO_TICKS(10000)) {
            checkSchedule();
            lastCheck = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void PureSpaService::checkSchedule() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Only check once per minute
    if (timeinfo.tm_min == _lastCheckedMinute) return;
    _lastCheckedMinute = timeinfo.tm_min;

    // Check if time is actually set (year > 2020)
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "Time not set, skipping schedule check");
        return;
    }

    ESP_LOGI(TAG, "Checking schedule for %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    std::lock_guard<std::recursive_mutex> lock(_eventsMutex);
    for (auto it = _events.begin(); it != _events.end(); ) {
        bool trigger = false;
        if (it->enabled && it->hour == timeinfo.tm_hour && it->minute == timeinfo.tm_min) {
            if (it->recurring) {
                if (it->dayOfWeekMask & (1 << timeinfo.tm_wday)) {
                    trigger = true;
                }
            } else {
                if (it->year == (timeinfo.tm_year + 1900) && 
                    it->month == (timeinfo.tm_mon + 1) && 
                    it->day == timeinfo.tm_mday) {
                    trigger = true;
                }
            }
        }

        if (trigger) {
            ESP_LOGI(TAG, "Triggering event ID %d", it->id);
            executeEvent(*it);
            if (!it->recurring) {
                // Remove non-recurring event after it trigger
                it = _events.erase(it);
                saveSchedule(); // Persist the removal
                continue;
            }
        }
        ++it;
    }
}

void PureSpaService::executeEvent(const ScheduledEvent& event) {
    bool isCurrentlyOn = _io.isPowerOn();

    // 1. Check if we need to auto power-on (only if we want to turn something ON)
    bool needsPowerOn = (event.setFilter && event.filterValue) || 
                        (event.setHeater && event.heaterValue) || 
                        (event.setBubble && event.bubbleValue) ||
                        (event.setTargetTemp);

    if (needsPowerOn && !isCurrentlyOn) {
        ESP_LOGI(TAG, "Auto powering ON for scheduled task");
        setPower(true);
        isCurrentlyOn = true; // Assume it will turn on for subsequent command checks
    }

    // 2. Execute commands
    if (event.setPower) setPower(event.powerValue);
    
    // Only execute feature commands if power is ON (either it was already ON, or we just turned it ON)
    if (isCurrentlyOn) {
        if (event.setFilter) setFilter(event.filterValue);
        if (event.setHeater) setHeater(event.heaterValue);
        if (event.setBubble) setBubble(event.bubbleValue);
        if (event.setTargetTemp) setTargetTemp(event.targetTempValue);
    } else {
        ESP_LOGI(TAG, "Skipping feature commands as Power is OFF and no 'ON' trigger was present");
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

    // Add current time for UI
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > (2020 - 1900)) {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        cJSON_AddStringToObject(root, "time", strftime_buf);
    } else {
        cJSON_AddStringToObject(root, "time", "Not set");
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    std::string result = json_str ? json_str : "{}";
    
    if (json_str) free(json_str);
    cJSON_Delete(root);
    
    return result;
}

std::string PureSpaService::getScheduleJson() {
    std::lock_guard<std::recursive_mutex> lock(_eventsMutex);
    cJSON *root = cJSON_CreateArray();
    for (const auto& ev : _events) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", ev.id);
        cJSON_AddBoolToObject(item, "enabled", ev.enabled);
        cJSON_AddBoolToObject(item, "recurring", ev.recurring);
        cJSON_AddNumberToObject(item, "dow", ev.dayOfWeekMask);
        cJSON_AddNumberToObject(item, "year", ev.year);
        cJSON_AddNumberToObject(item, "month", ev.month);
        cJSON_AddNumberToObject(item, "day", ev.day);
        cJSON_AddNumberToObject(item, "hour", ev.hour);
        cJSON_AddNumberToObject(item, "minute", ev.minute);
        
        cJSON_AddBoolToObject(item, "setPower", ev.setPower);
        cJSON_AddBoolToObject(item, "powerValue", ev.powerValue);
        cJSON_AddBoolToObject(item, "setFilter", ev.setFilter);
        cJSON_AddBoolToObject(item, "filterValue", ev.filterValue);
        cJSON_AddBoolToObject(item, "setHeater", ev.setHeater);
        cJSON_AddBoolToObject(item, "heaterValue", ev.heaterValue);
        cJSON_AddBoolToObject(item, "setBubble", ev.setBubble);
        cJSON_AddBoolToObject(item, "bubbleValue", ev.bubbleValue);
        cJSON_AddBoolToObject(item, "setTemp", ev.setTargetTemp);
        cJSON_AddNumberToObject(item, "tempValue", ev.targetTempValue);
        
        cJSON_AddItemToArray(root, item);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    std::string result = json_str ? json_str : "[]";
    if (json_str) free(json_str);
    cJSON_Delete(root);
    return result;
}

void PureSpaService::addEvent(const ScheduledEvent& event) {
    std::lock_guard<std::recursive_mutex> lock(_eventsMutex);
    if (_events.size() >= 10) {
        ESP_LOGW(TAG, "Maximum number of events (10) reached. Cannot add more.");
        return;
    }
    ScheduledEvent newEvent = event;
    newEvent.id = _nextEventId++;
    _events.push_back(newEvent);
    ESP_LOGI(TAG, "Added new event ID %ld: Time %02d:%02d, Recurring: %d", (long)newEvent.id, newEvent.hour, newEvent.minute, newEvent.recurring);
    saveSchedule();
}

void PureSpaService::deleteEvent(int id) {
    std::lock_guard<std::recursive_mutex> lock(_eventsMutex);
    size_t oldSize = _events.size();
    _events.erase(std::remove_if(_events.begin(), _events.end(), [id](const ScheduledEvent& e) {
        return e.id == id;
    }), _events.end());
    if (_events.size() < oldSize) {
        ESP_LOGI(TAG, "Deleted event ID %d", id);
    } else {
        ESP_LOGW(TAG, "Failed to delete event ID %d: Not found", id);
    }
    saveSchedule();
}

void PureSpaService::toggleEvent(int id, bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(_eventsMutex);
    bool found = false;
    for (auto& ev : _events) {
        if (ev.id == id) {
            ev.enabled = enabled;
            found = true;
            ESP_LOGI(TAG, "Event ID %d %s", id, enabled ? "enabled" : "disabled");
            break;
        }
    }
    if (!found) ESP_LOGW(TAG, "Failed to toggle event ID %d: Not found", id);
    saveSchedule();
}

void PureSpaService::saveSchedule() {
    ESP_LOGI(TAG, "Saving schedule to NVS...");
    std::string json = getScheduleJson();
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SCHEDULE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, SCHEDULE_KEY, json.c_str());
        nvs_set_i32(nvs_handle, "next_id", _nextEventId);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Schedule saved successfully (%d bytes)", (int)json.length());
    } else {
        ESP_LOGE(TAG, "Error opening NVS for saving: %s", esp_err_to_name(err));
    }
}

void PureSpaService::loadSchedule() {
    ESP_LOGI(TAG, "Loading schedule from NVS...");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SCHEDULE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size;
        nvs_get_str(nvs_handle, SCHEDULE_KEY, NULL, &required_size);
        char* json_buf = (char*)malloc(required_size);
        nvs_get_str(nvs_handle, SCHEDULE_KEY, json_buf, &required_size);
        nvs_get_i32(nvs_handle, "next_id", &_nextEventId);
        nvs_close(nvs_handle);

        cJSON *root = cJSON_Parse(json_buf);
        if (root && cJSON_IsArray(root)) {
            _events.clear();
            int n = cJSON_GetArraySize(root);
            for (int i = 0; i < n; i++) {
                cJSON *item = cJSON_GetArrayItem(root, i);
                ScheduledEvent ev;
                ev.id = cJSON_GetObjectItem(item, "id")->valueint;
                ev.enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"));
                ev.recurring = cJSON_IsTrue(cJSON_GetObjectItem(item, "recurring"));
                ev.dayOfWeekMask = cJSON_GetObjectItem(item, "dow")->valueint;
                ev.year = cJSON_GetObjectItem(item, "year")->valueint;
                ev.month = cJSON_GetObjectItem(item, "month")->valueint;
                ev.day = cJSON_GetObjectItem(item, "day")->valueint;
                ev.hour = cJSON_GetObjectItem(item, "hour")->valueint;
                ev.minute = cJSON_GetObjectItem(item, "minute")->valueint;
                
                ev.setPower = cJSON_IsTrue(cJSON_GetObjectItem(item, "setPower"));
                ev.powerValue = cJSON_IsTrue(cJSON_GetObjectItem(item, "powerValue"));
                ev.setFilter = cJSON_IsTrue(cJSON_GetObjectItem(item, "setFilter"));
                ev.filterValue = cJSON_IsTrue(cJSON_GetObjectItem(item, "filterValue"));
                ev.setHeater = cJSON_IsTrue(cJSON_GetObjectItem(item, "setHeater"));
                ev.heaterValue = cJSON_IsTrue(cJSON_GetObjectItem(item, "heaterValue"));
                ev.setBubble = cJSON_IsTrue(cJSON_GetObjectItem(item, "setBubble"));
                ev.bubbleValue = cJSON_IsTrue(cJSON_GetObjectItem(item, "bubbleValue"));
                ev.setTargetTemp = cJSON_IsTrue(cJSON_GetObjectItem(item, "setTemp"));
                ev.targetTempValue = cJSON_GetObjectItem(item, "tempValue")->valueint;
                
                _events.push_back(ev);
            }
            ESP_LOGI(TAG, "Loaded %d events from NVS. Next ID: %ld", n, (long)_nextEventId);
        }
        if (root) cJSON_Delete(root);
        free(json_buf);
    } else {
        ESP_LOGI(TAG, "No schedule found in NVS (or first boot)");
    }
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
