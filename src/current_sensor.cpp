#include "current_sensor.h"
#include "board_config.h"
#include "calibration.h"
#include <Adafruit_INA237.h>
#include <Wire.h>

CurrentSensor g_current;
static Adafruit_INA237 ina237;

bool CurrentSensor::begin() {
    g_calib.begin();
    _healthy = ina237.begin(INA237_I2C_ADDR);
    if (_healthy) {
        ina237.setShunt(SHUNT_RESISTOR_OHM, MAX_CURRENT_A);
    }
    return _healthy;
}

float CurrentSensor::readRawAmps() {
    if (!_healthy) return 0.0f;

    float amps = ina237.readCurrent();
    if (amps < 0.0f) amps = 0.0f;
    if (amps > MAX_CURRENT_A) amps = MAX_CURRENT_A;
    return amps;
}

float CurrentSensor::readShuntVoltageV() {
    if (!_healthy) return -1.0f;
    return ina237.readShuntVoltage();
}

float CurrentSensor::readShuntVoltage_mV() {
    float v = readShuntVoltageV();
    if (v < 0.0f) return v;
    return v * 1000.0f;
}

float CurrentSensor::readCurrentA() {
    float raw = readRawAmps();
    float calibrated = g_calib.apply(raw);
    _lastCurrent = 0.7f * _lastCurrent + 0.3f * calibrated;
    return _lastCurrent;
}
