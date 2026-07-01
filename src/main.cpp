#include <Arduino.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>

#include "alarm_manager.h"
#include "ble_service.h"
#include "board_config.h"
#include "calibration.h"
#include "current_sensor.h"
#include "data_logger.h"
#include "i2c_manager.h"
#include "indicators.h"
#include "lab_test.h"
#include "power_monitor.h"
#include "serial_cli.h"
#include "temp_sensors.h"
#include "time_service.h"

static LiveData g_live{};
static uint32_t s_lastMeasureMs   = 0;
static uint32_t s_lastLogMs       = 0;
static uint32_t s_lastBleMs       = 0;
static uint32_t s_lastIndicatorMs = 0;

static AlarmRecord* s_psramAlarms = nullptr;
static size_t       s_psramAlarmCount = 0;
static const size_t PSRAM_ALARM_MAX = 50;

void onAlarmTriggered(const AlarmRecord& rec) {
    g_logger.logAlarm(rec);
    if (s_psramAlarms && s_psramAlarmCount < PSRAM_ALARM_MAX) {
        s_psramAlarms[s_psramAlarmCount++] = rec;
    }
}

static void logBootEvent() {
    SystemEventRecord ev{};
    ev.timestamp_ms = g_time.nowMs();
    ev.type = SystemEventType::BOOT;
    g_logger.logSystemEvent(ev);
}

static void handlePowerChange() {
    if (!g_power.hasSourceChanged()) return;

    SystemEventRecord ev{};
    ev.timestamp_ms = g_time.nowMs();
    ev.type = SystemEventType::POWER_CHANGE;
    ev.param = (uint8_t)g_power.readSource();
    g_logger.logSystemEvent(ev);

    if (g_power.readSource() == PowerSource::USB_C) {
        SystemEventRecord usb{};
        usb.timestamp_ms = g_time.nowMs();
        usb.type = SystemEventType::TECH_USB_CONN;
        g_logger.logSystemEvent(usb);
    }
}

static void acquireMeasurements() {
    if (!g_lab.isActive()) {
        g_live.current_a      = g_current.readCurrentA();
        g_live.temp_pcb1_c    = g_temp.readPcb1C();
        g_live.temp_pcb2_c    = g_temp.readPcb2C();
        g_live.temp_ambient_c = g_temp.readAmbientC();
    }
    g_lab.applyTo(g_live);

    g_live.humidity_pct      = g_i2c.readHumidityPct();
    g_live.power_source      = g_power.readSource();
    g_live.vibration_present = g_i2c.vibrationPresent();
    g_live.humidity_present  = g_i2c.humidityPresent();
    g_live.lora_present      = g_i2c.loraPresent();

    g_alarm.update(g_live.current_a, g_live.temp_pcb1_c, g_live.temp_pcb2_c);
    g_live.alarm_level = g_alarm.level();
}

static void persistMeasurement() {
    MeasurementRecord rec{};
    rec.timestamp_ms   = g_time.nowMs();
    rec.current_a      = g_live.current_a;
    rec.temp_pcb1_c    = g_live.temp_pcb1_c;
    rec.temp_pcb2_c    = g_live.temp_pcb2_c;
    rec.temp_ambient_c = g_live.temp_ambient_c;
    rec.humidity_pct   = g_live.humidity_pct;
    rec.alarm_level    = (uint8_t)g_live.alarm_level;
    rec.power_source   = (uint8_t)g_live.power_source;
    g_logger.logMeasurement(rec);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Ventec AGV Monitor v1.2 - demarrage");

    if (psramFound()) {
        Serial.printf("PSRAM detectee : %u Ko\n", ESP.getPsramSize() / 1024);
    } else {
        Serial.println("ATTENTION: PSRAM non detectee (verifier board_build.psram et module WROVER)");
    }

    s_psramAlarms = (AlarmRecord*)heap_caps_malloc(
        PSRAM_ALARM_MAX * sizeof(AlarmRecord), MALLOC_CAP_SPIRAM);
    if (s_psramAlarms) {
        Serial.println("Tampon alarmes : PSRAM");
    } else {
        s_psramAlarms = (AlarmRecord*)malloc(PSRAM_ALARM_MAX * sizeof(AlarmRecord));
        Serial.println("Tampon alarmes : RAM interne (fallback)");
    }

    g_time.begin();
    g_i2c.begin();
    g_power.begin();
    g_current.begin();
    g_temp.begin();
    g_logger.begin();
    g_alarm.begin();
    g_indicators.begin();
    g_ble.begin();

    logBootEvent();

    if (!g_current.isHealthy()) {
        SystemEventRecord ev{};
        ev.timestamp_ms = g_time.nowMs();
        ev.type = SystemEventType::SENSOR_FAULT;
        ev.param = 1;
        g_logger.logSystemEvent(ev);
        Serial.println("ATTENTION: INA219 non detecte (verifier I2C)");
    }

    Serial.println("Pret. Tapez HELP pour les commandes.");
}

void loop() {
    uint32_t now = millis();

    handlePowerChange();
    g_cli.process(g_live);

    if (now - s_lastMeasureMs >= MEASURE_INTERVAL_MS) {
        s_lastMeasureMs = now;
        acquireMeasurements();
        g_ble.updateLiveData(g_live);
    }

    if (now - s_lastLogMs >= LOG_INTERVAL_MS) {
        s_lastLogMs = now;
        persistMeasurement();
    }

    if (now - s_lastBleMs >= BLE_NOTIFY_INTERVAL_MS) {
        s_lastBleMs = now;
        g_ble.notifyIfConnected();
    }

    if (now - s_lastIndicatorMs >= 500) {
        s_lastIndicatorMs = now;
        g_indicators.update(g_live.alarm_level);
    }
}
