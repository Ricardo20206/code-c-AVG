#pragma once

#include "types.h"

class PowerMonitor {
public:
    bool begin();
    PowerSource readSource() const;
    const char* sourceName(PowerSource src) const;
    bool hasSourceChanged();
    float readBatteryPercent();
    float readBatteryVoltage();

private:
    void update();

    PowerSource _lastSource = PowerSource::UNKNOWN;
    float _batteryVoltage = 36.0f;
    float _batteryPercent = 100.0f;
};

extern PowerMonitor g_power;