#pragma once

#include "types.h"
#include <Preferences.h>

class AlarmManager {
public:
    bool begin();
    void update(float currentA, float tempPcb1, float tempPcb2);
    AlarmLevel level() const { return _level; }
    bool levelChanged() const { return _levelChanged; }
    void clearLevelChanged() { _levelChanged = false; }

    AlarmThresholds thresholds() const { return _thresholds; }
    bool setThresholds(const AlarmThresholds& t);
    void loadThresholds();
    void saveThresholds();

    float baselineCurrent() const { return _baselineCurrent; }

private:
    void evaluateCurrent(float currentA);
    void evaluateTemperature(float t1, float t2);
    void setLevel(AlarmLevel lvl);

    AlarmThresholds _thresholds{};
    AlarmLevel      _level = AlarmLevel::NONE;
    AlarmLevel      _prevLevel = AlarmLevel::NONE;
    bool            _levelChanged = false;

    uint32_t _warnStartMs = 0;
    uint32_t _alarmStartMs = 0;
    float    _baselineCurrent = 2.0f;
    uint32_t _baselineUpdateMs = 0;

    Preferences _prefs;
};

extern AlarmManager g_alarm;

// Callback enregistrement alarme (défini dans main)
void onAlarmTriggered(const AlarmRecord& rec);
