#include "time_service.h"
#include "board_config.h"
#include <RTClib.h>
#include <Wire.h>

TimeService g_time;
static RTC_DS3231 s_rtc;
static bool s_rtcOk = false;

bool TimeService::begin() {
    s_rtcOk = s_rtc.begin();
    if (s_rtcOk) {
        if (s_rtc.lostPower()) {
            s_rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        _rtcAvailable = true;
    }
    return true;
}

uint32_t TimeService::nowMs() const {
    if (_rtcAvailable && s_rtcOk) {
        DateTime now = s_rtc.now();
        return (uint32_t)now.unixtime() * 1000UL;
    }
    return millis();
}

void TimeService::syncFromRtc() {
    begin();
}
