#include "AuditLogger.h"
#include <esp_log.h>
#include "nvs_flash.h"
#include <cstring>
#include <algorithm>

static const char* TAG = "AuditLogger";
static const char* NVS_NAMESPACE = "audit_trail";

void AuditLogger::init() {
    loadFromNvs();
    pruneOldEvents();
}

void AuditLogger::logEvent(const char* source, const char* feature, bool state) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    AuditEvent event;
    time(&event.timestamp);
    
    // Safety boundaries for string buffers
    strncpy(event.source, source, sizeof(event.source) - 1);
    event.source[sizeof(event.source) - 1] = '\0';
    
    strncpy(event.feature, feature, sizeof(event.feature) - 1);
    event.feature[sizeof(event.feature) - 1] = '\0';
    
    event.state = state;

    // Prune before inserting if list exceeds safe limit
    if (_events.size() >= 100) {
        // Remove oldest event
        _events.erase(_events.begin());
    }

    _events.push_back(event);
    ESP_LOGI(TAG, "Logged event: %s changed %s to %s", source, feature, state ? "ON" : "OFF");

    saveToNvs();
}

std::vector<AuditEvent> AuditLogger::getEvents() {
    std::lock_guard<std::mutex> lock(_mutex);
    pruneOldEvents(); // Prune on retrieval to ensure client gets fresh logs
    return _events;
}

void AuditLogger::setRetentionDays(int days) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (days < 1) days = 1;
    if (days > 7) days = 7;
    _retentionDays = days;
    ESP_LOGI(TAG, "Retention changed to %d days", _retentionDays);
    
    // Persist configuration
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_i32(my_handle, "retention", _retentionDays);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
    
    pruneOldEvents();
    saveToNvs();
}

void AuditLogger::clearLog() {
    std::lock_guard<std::mutex> lock(_mutex);
    _events.clear();
    ESP_LOGI(TAG, "Audit log cleared");
    saveToNvs();
}

void AuditLogger::pruneOldEvents() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Only prune if the system time is synchronized (year > 2020)
    // If not synchronized, timeinfo.tm_year is since 1900 (so < 120 means < 2020)
    if (timeinfo.tm_year < 120) {
        ESP_LOGD(TAG, "System time is not synchronized (year < 2020). Skipping pruning.");
        return;
    }

    time_t cutoff = now - (_retentionDays * 24 * 3600);
    size_t initial_size = _events.size();

    _events.erase(
        std::remove_if(_events.begin(), _events.end(), [cutoff](const AuditEvent& e) {
            return e.timestamp < cutoff;
        }),
        _events.end()
    );

    if (_events.size() != initial_size) {
        ESP_LOGI(TAG, "Pruned %d expired events older than %d days", (int)(initial_size - _events.size()), _retentionDays);
        saveToNvs();
    }
}

void AuditLogger::loadFromNvs() {
    std::lock_guard<std::mutex> lock(_mutex);
    _events.clear();
    
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No audit trail namespace found in NVS (first boot)");
        return;
    }

    // Load retention config
    int32_t retention = 7;
    if (nvs_get_i32(my_handle, "retention", &retention) == ESP_OK) {
        _retentionDays = retention;
    }

    // Load logs blob
    size_t required_size = 0;
    err = nvs_get_blob(my_handle, "logs", NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        size_t event_count = required_size / sizeof(AuditEvent);
        _events.resize(event_count);
        nvs_get_blob(my_handle, "logs", _events.data(), &required_size);
        ESP_LOGI(TAG, "Loaded %d audit events from NVS", (int)event_count);
    }
    
    nvs_close(my_handle);
}

void AuditLogger::saveToNvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS namespace for saving (%s)", esp_err_to_name(err));
        return;
    }

    if (_events.empty()) {
        nvs_erase_key(my_handle, "logs");
    } else {
        err = nvs_set_blob(my_handle, "logs", _events.data(), _events.size() * sizeof(AuditEvent));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error saving events blob to NVS (%s)", esp_err_to_name(err));
        }
    }
    
    nvs_commit(my_handle);
    nvs_close(my_handle);
}
