#include "current_sensor.h"
#include "board_config.h"
#include "calibration.h"
#include <Adafruit_INA219.h>
#include <Wire.h>

CurrentSensor g_current;
static Adafruit_INA219 ina219(INA219_I2C_ADDR);

bool CurrentSensor::begin() {
    g_calib.begin();
    _healthy = ina219.begin();
    if (_healthy) {
        ina219.setCalibration_32V_2A();
    }
    return _healthy;
}

float CurrentSensor::readRawAmps() {
    if (!_healthy) return 0.0f;

    float mA = ina219.getCurrent_mA();
    float scale = 0.1f / SHUNT_RESISTOR_OHM;
    float amps = (mA / 1000.0f) * scale;
    if (amps < 0.0f) amps = 0.0f;
    if (amps > MAX_CURRENT_A) amps = MAX_CURRENT_A;
    return amps;
}

float CurrentSensor::readCurrentA() {
    float raw = readRawAmps();
    float calibrated = g_calib.apply(raw);
    _lastCurrent = 0.7f * _lastCurrent + 0.3f * calibrated;
    return _lastCurrent;
}
