#pragma once

#include "types.h"

class PowerMonitor {
public:
    bool begin();
    PowerSource readSource() const;
    const char* sourceName(PowerSource src) const;
    bool hasSourceChanged();

private:
    PowerSource _lastSource = PowerSource::UNKNOWN;
};

extern PowerMonitor g_power;
