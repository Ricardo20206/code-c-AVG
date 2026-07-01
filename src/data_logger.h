#pragma once

#include "types.h"
#include <Arduino.h>
#include <vector>

class DataLogger {
public:
    bool begin();
    void logMeasurement(const MeasurementRecord& rec);
    void logAlarm(const AlarmRecord& rec);
    void logSystemEvent(const SystemEventRecord& ev);

    size_t measurementCount() const { return _measCount; }
    size_t alarmCount() const { return _alarmCount; }
    size_t systemCount() const { return _sysCount; }

    std::vector<MeasurementRecord> getLastMeasurements(size_t n) const;
    std::vector<AlarmRecord>       getLastAlarms(size_t n) const;
    std::vector<SystemEventRecord> getLastSystemEvents(size_t n) const;

    String exportAllCsv() const;
    void clearAll();

private:
    size_t _measCount = 0;
    size_t _alarmCount = 0;
    size_t _sysCount = 0;
    bool   _ready = false;
};

extern DataLogger g_logger;
