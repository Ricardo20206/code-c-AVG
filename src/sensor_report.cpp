#include "sensor_report.h"
#include "power_monitor.h"
#include <Arduino.h>

namespace {

const char* alarmLevelName(AlarmLevel lvl) {
    switch (lvl) {
        case AlarmLevel::WARNING:  return "WARNING";
        case AlarmLevel::CRITICAL: return "CRITIQUE";
        default:                   return "OK";
    }
}

} // namespace

void printSensorReport(const LiveData& live, float vibrationMg, bool inaOk, bool tmpOk) {
    const char* alm = alarmLevelName(live.alarm_level);
    const char* src = g_power.sourceName(live.power_source);

    Serial.println("--- Capteurs Ventec ---");
    Serial.printf("  Courant batterie : %.2f A\n", live.current_a);
    Serial.printf("  Temperature PCB1  : %.1f C\n", live.temp_pcb1_c);
    Serial.printf("  Temperature PCB2  : %.1f C\n", live.temp_pcb2_c);
    Serial.printf("  Temperature amb.  : %.1f C\n", live.temp_ambient_c);
    if (live.humidity_pct >= 0) {
        Serial.printf("  Humidite          : %.1f %%\n", live.humidity_pct);
    } else {
        Serial.println("  Humidite          : N/A");
    }
    if (vibrationMg >= 0) {
        Serial.printf("  Vibration         : %.0f mg\n", vibrationMg);
    } else {
        Serial.println("  Vibration         : N/A");
    }
    Serial.printf("  Alarme            : %s\n", alm);
    Serial.printf("  Source alim.      : %s\n", src);
    Serial.printf("  INA237 (courant)  : %s\n", inaOk ? "OK" : "ABSENT");
    Serial.printf("  TMP126 (ambiant)  : %s\n", tmpOk ? "OK" : "ABSENT");
    Serial.printf("  Grove vibration   : %s\n", live.vibration_present ? "PRESENT" : "ABSENT");
    Serial.printf("  Grove humidite    : %s\n", live.humidity_present ? "PRESENT" : "ABSENT");
    Serial.printf("  MikroBus LoRa     : %s\n", live.lora_present ? "PRESENT" : "ABSENT");
    Serial.printf("  Mode labo         : %s\n", live.lab_active ? "OUI" : "NON");

    Serial.printf(
        "[SENSORS] I=%.2f T1=%.1f T2=%.1f AMB=%.1f HUM=%.1f VIB=%.0f ALM=%d SRC=%s "
        "INA=%d TMP=%d VIB_S=%d HUM_S=%d LAB=%d\n",
        live.current_a,
        live.temp_pcb1_c,
        live.temp_pcb2_c,
        live.temp_ambient_c,
        live.humidity_pct >= 0 ? live.humidity_pct : -1.0f,
        vibrationMg >= 0 ? vibrationMg : -1.0f,
        (int)live.alarm_level,
        src,
        inaOk ? 1 : 0,
        tmpOk ? 1 : 0,
        live.vibration_present ? 1 : 0,
        live.humidity_present ? 1 : 0,
        live.lab_active ? 1 : 0);
}
