#pragma once

#include "types.h"

class TempSensors {
public:
    bool begin();
    float readPcb1C();
    float readPcb2C();
    float readAmbientC();
    bool  ambientHealthy() const { return _ambientOk; }

private:
    float ntcAdcToCelsius(int adcPin);
    bool  _ambientOk = false;
};

extern TempSensors g_temp;
