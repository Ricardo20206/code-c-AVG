#pragma once

#include "types.h"
#include <cstdint>

class TimeService {
public:
    bool begin();
    uint32_t nowMs() const;
    void syncFromRtc();

private:
    uint32_t _bootOffsetMs = 0;
    bool     _rtcAvailable = false;
};

extern TimeService g_time;
