#pragma once

#include <cstdint>

enum class PowerSource : uint8_t {
    UNKNOWN = 0,
    HT_36V  = 1,   // Batterie principale haute tension
    BT_SVC  = 2,   // Batterie externe de service
    USB_C   = 3    // Alimentation USB-C technicien
};

enum class AlarmLevel : uint8_t {
    NONE     = 0,
    WARNING  = 1,  // Courant > 15A continu
    CRITICAL = 2   // Courant > 20A continu ou T° PCB > 75°C
};

enum class AlarmType : uint8_t {
    CURRENT_HIGH  = 1,
    TEMP_PCB_HIGH = 2,
    CURRENT_DRIFT = 3
};

enum class SystemEventType : uint8_t {
    BOOT            = 1,
    POWER_CHANGE    = 2,
    TECH_USB_CONN   = 3,
    TECH_BLE_CONN   = 4,
    THRESHOLD_CHANGE = 5,
    SENSOR_FAULT    = 6
};

struct MeasurementRecord {
    uint32_t timestamp_ms;
    float    current_a;
    float    temp_pcb1_c;
    float    temp_pcb2_c;
    float    temp_ambient_c;
    float    humidity_pct;     // -1 si capteur absent
    uint8_t  alarm_level;
    uint8_t  power_source;
};

struct AlarmRecord {
    uint32_t    timestamp_ms;
    AlarmType   type;
    AlarmLevel  level;
    float       trigger_value;
    float       threshold;
};

struct SystemEventRecord {
    uint32_t         timestamp_ms;
    SystemEventType  type;
    uint8_t          param;
};

struct AlarmThresholds {
    float current_warn_a;
    float current_alarm_a;
    float temp_alarm_c;
    float current_drift_a;
};

struct LiveData {
    float         current_a;
    float         temp_pcb1_c;
    float         temp_pcb2_c;
    float         temp_ambient_c;
    float         humidity_pct;
    AlarmLevel    alarm_level;
    PowerSource   power_source;
    bool          vibration_present;
    bool          humidity_present;
    bool          lora_present;
    bool          lab_active;
};
