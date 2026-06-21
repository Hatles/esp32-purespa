#ifndef AUDIT_LOGGER_H
#define AUDIT_LOGGER_H

#include <vector>
#include <mutex>
#include <time.h>
#include "esp_err.h"

struct AuditEvent {
    time_t timestamp;
    char source[16];   // e.g., "Web UI", "Schedule #2"
    char feature[12];  // "Power", "Filter", "Heater", "Bubbles"
    bool state;        // true = ON, false = OFF
};

class AuditLogger {
public:
    static AuditLogger& getInstance() {
        static AuditLogger instance;
        return instance;
    }

    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;

    void init();
    void logEvent(const char* source, const char* feature, bool state);
    std::vector<AuditEvent> getEvents();
    
    void setRetentionDays(int days);
    int getRetentionDays() const { return _retentionDays; }
    
    void clearLog();
    void pruneOldEvents();

private:
    AuditLogger() : _retentionDays(7) {}

    void loadFromNvs();
    void saveToNvs();

    std::vector<AuditEvent> _events;
    mutable std::mutex _mutex;
    int _retentionDays;
};

#endif // AUDIT_LOGGER_H
