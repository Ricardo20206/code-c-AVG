#include "alarm_manager.h"
#include "board_config.h"
#include "time_service.h"
#include <Arduino.h>

AlarmManager g_alarm;

bool AlarmManager::begin() {
    _thresholds.current_warn_a  = DEFAULT_CURRENT_WARN_A;
    _thresholds.current_alarm_a = DEFAULT_CURRENT_ALARM_A;
    _thresholds.temp_alarm_c    = DEFAULT_TEMP_ALARM_C;
    _thresholds.current_drift_a = DEFAULT_CURRENT_DRIFT_A;
    loadThresholds();
    _baselineUpdateMs = millis();
    return true;
}

void AlarmManager::loadThresholds() {
    _prefs.begin(CONFIG_NAMESPACE, true);
    _thresholds.current_warn_a  = _prefs.getFloat("cur_warn",  DEFAULT_CURRENT_WARN_A);
    _thresholds.current_alarm_a = _prefs.getFloat("cur_alarm", DEFAULT_CURRENT_ALARM_A);
    _thresholds.temp_alarm_c    = _prefs.getFloat("temp_alm",  DEFAULT_TEMP_ALARM_C);
    _thresholds.current_drift_a = _prefs.getFloat("cur_drift", DEFAULT_CURRENT_DRIFT_A);
    _prefs.end();
}

void AlarmManager::saveThresholds() {
    _prefs.begin(CONFIG_NAMESPACE, false);
    _prefs.putFloat("cur_warn",  _thresholds.current_warn_a);
    _prefs.putFloat("cur_alarm", _thresholds.current_alarm_a);
    _prefs.putFloat("temp_alm",  _thresholds.temp_alarm_c);
    _prefs.putFloat("cur_drift", _thresholds.current_drift_a);
    _prefs.end();
}

bool AlarmManager::setThresholds(const AlarmThresholds& t) {
    if (t.current_warn_a >= t.current_alarm_a) return false;
    if (t.temp_alarm_c < 40.0f || t.temp_alarm_c > 120.0f) return false;
    _thresholds = t;
    saveThresholds();
    return true;
}

void AlarmManager::setLevel(AlarmLevel lvl) {
    if (lvl != _level) {
        _level = lvl;
        _levelChanged = true;
    }
}

void AlarmManager::evaluateCurrent(float currentA) {
    uint32_t now = millis();

    if (now - _baselineUpdateMs > 60000) {
        _baselineCurrent = 0.9f * _baselineCurrent + 0.1f * currentA;
        _baselineUpdateMs = now;
    }

    bool criticalCurrent = false;
    bool warningCurrent  = false;

    if (currentA >= _thresholds.current_alarm_a) {
        if (_alarmStartMs == 0) _alarmStartMs = now;
        criticalCurrent = (now - _alarmStartMs >= CURRENT_ALARM_HOLD_MS);
    } else {
        _alarmStartMs = 0;
    }

    if (!criticalCurrent && currentA >= _thresholds.current_warn_a) {
        if (_warnStartMs == 0) _warnStartMs = now;
        warningCurrent = (now - _warnStartMs >= CURRENT_ALARM_HOLD_MS);
    } else if (!criticalCurrent) {
        _warnStartMs = 0;
    }

    if (criticalCurrent) {
        setLevel(AlarmLevel::CRITICAL);
    } else if (warningCurrent) {
        setLevel(AlarmLevel::WARNING);
    }
}

void AlarmManager::evaluateTemperature(float t1, float t2) {
    float maxPcb = (t1 > t2) ? t1 : t2;
    if (maxPcb >= _thresholds.temp_alarm_c) {
        setLevel(AlarmLevel::CRITICAL);
    }
}

void AlarmManager::update(float currentA, float tempPcb1, float tempPcb2) {
    AlarmLevel prev = _level;
    _level = AlarmLevel::NONE;

    evaluateCurrent(currentA);
    evaluateTemperature(tempPcb1, tempPcb2);

    if (_level == AlarmLevel::NONE && prev != AlarmLevel::NONE) {
        _levelChanged = true;
    }

    if (_level != prev && _level != AlarmLevel::NONE) {
        AlarmRecord rec{};
        rec.timestamp_ms = g_time.nowMs();
        rec.level = _level;

        if (_level == AlarmLevel::CRITICAL) {
            float maxPcb = (tempPcb1 > tempPcb2) ? tempPcb1 : tempPcb2;
            if (maxPcb >= _thresholds.temp_alarm_c) {
                rec.type = AlarmType::TEMP_PCB_HIGH;
                rec.trigger_value = maxPcb;
                rec.threshold = _thresholds.temp_alarm_c;
            } else {
                rec.type = AlarmType::CURRENT_HIGH;
                rec.trigger_value = currentA;
                rec.threshold = _thresholds.current_alarm_a;
            }
        } else {
            rec.type = AlarmType::CURRENT_HIGH;
            rec.trigger_value = currentA;
            rec.threshold = _thresholds.current_warn_a;
        }
        onAlarmTriggered(rec);
    }
}
