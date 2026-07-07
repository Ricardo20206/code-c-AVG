#pragma once

#include "types.h"

class CurrentSensor {
public:
    bool begin();
    float readCurrentA();
    float readRawAmps();
    float readShuntVoltageV();
    float readShuntVoltage_mV();
    bool isHealthy() const { return _healthy; }

private:
    bool  _healthy = false;
    float _lastCurrent = 0.0f;
};

extern CurrentSensor g_current;
