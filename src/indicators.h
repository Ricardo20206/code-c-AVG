#pragma once

#include "types.h"

class Indicators {
public:
    bool begin();
    void update(AlarmLevel level);
    void playTone(uint16_t freqHz, uint16_t durationMs);
    void playPattern(AlarmLevel level);

private:
    void setGreen(bool on);
    void setRed(bool on);
    void setBuzzer(bool on);

    AlarmLevel _lastLevel = AlarmLevel::NONE;
    uint32_t   _blinkMs = 0;
    bool       _blinkState = false;
};

extern Indicators g_indicators;
