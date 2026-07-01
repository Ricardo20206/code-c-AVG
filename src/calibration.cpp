#include "calibration.h"
#include "board_config.h"
#include <Preferences.h>

CurrentCalibration g_calib;

bool CurrentCalibration::begin() {
    load();
    return true;
}

void CurrentCalibration::load() {
    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE, true);
    _offset = prefs.getFloat("cal_offset", 0.0f);
    _gain   = prefs.getFloat("cal_gain",   1.0f);
    prefs.end();
}

void CurrentCalibration::save() {
    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE, false);
    prefs.putFloat("cal_offset", _offset);
    prefs.putFloat("cal_gain",   _gain);
    prefs.end();
}

float CurrentCalibration::apply(float rawAmps) const {
    return (rawAmps - _offset) * _gain;
}

bool CurrentCalibration::calibrateWithReference(float measuredRaw, float referenceA) {
    if (referenceA <= 0.0f) return false;
    // Point unique : ajuste gain, conserve offset existant
    float corrected = measuredRaw - _offset;
    if (corrected < 0.01f) return false;
    _gain = referenceA / corrected;
    save();
    return true;
}

void CurrentCalibration::reset() {
    _offset = 0.0f;
    _gain   = 1.0f;
    save();
}
