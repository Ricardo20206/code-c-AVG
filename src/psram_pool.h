#pragma once

#include "types.h"
#include <cstddef>

class PsramPool {
public:
    bool begin();
    bool isActive() const { return _active; }
    bool isUsingPsram() const { return _usingPsram; }
    size_t totalBytes() const { return _totalBytes; }
    size_t freeBytes() const;

    void pushAlarm(const AlarmRecord& rec);
    void pushMeasurement(const MeasurementRecord& rec);
    size_t copyAlarms(AlarmRecord* out, size_t maxCount) const;

    void printStatus() const;

private:
    bool allocBuffers();

    bool   _active = false;
    bool   _usingPsram = false;
    size_t _totalBytes = 0;
    AlarmRecord*       _alarms = nullptr;
    size_t             _alarmCount = 0;
    MeasurementRecord* _measRing = nullptr;
    size_t             _measHead = 0;
    size_t             _measCount = 0;
};

extern PsramPool g_psram;

size_t ventecGetAlarmHistory(AlarmRecord* out, size_t maxCount);
