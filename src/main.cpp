#include <Arduino.h>
#include <esp_heap_caps.h>

#include "alarm_manager.h"
#include "ble_service.h"
#include "ble_snapshot.h"
#include "board_config.h"
#include "calibration.h"
#include "current_sensor.h"
#include "data_logger.h"
#include "i2c_manager.h"
#include "indicators.h"
#include "lab_test.h"
#include "power_monitor.h"
#include "serial_cli.h"
#include "psram_pool.h"
#include "sensor_report.h"
#include "temp_sensors.h"
#include "time_service.h"

static LiveData g_live{};
static float    s_vibrationMg = -1.0f;
static bool     s_monitorAuto = false;
static uint32_t s_lastMonitorMs = 0;
static uint32_t s_lastMeasureMs   = 0;
static uint32_t s_lastLogMs       = 0;
static uint32_t s_lastBleMs       = 0;
static uint32_t s_lastIndicatorMs = 0;

void onAlarmTriggered(const AlarmRecord& rec) {
    g_logger.logAlarm(rec);
    g_psram.pushAlarm(rec);
    g_ble.notifyAlarmEvent(rec);
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
    if (g_lab.isActive()) {
        g_lab.applyTo(g_live);
        s_vibrationMg = g_lab.simulatedVibrationMg();
    } else {
        g_live.lab_active = false;
        g_live.current_a      = g_current.readCurrentA();
        g_live.temp_pcb1_c    = g_temp.readPcb1C();
        g_live.temp_pcb2_c    = g_temp.readPcb2C();
        g_live.temp_ambient_c = g_temp.readAmbientC();
        g_live.humidity_pct      = g_i2c.readHumidityPct();
        g_live.vibration_present = g_i2c.vibrationPresent();
        g_live.humidity_present  = g_i2c.humidityPresent();
        g_live.lora_present      = g_i2c.loraPresent();
        g_live.power_source      = g_power.readSource();
        s_vibrationMg = g_i2c.readVibrationMg();
    }

    g_alarm.update(g_live.current_a, g_live.temp_pcb1_c, g_live.temp_pcb2_c);
    g_live.alarm_level = g_alarm.level();
}

static BleSnapshot buildBleSnapshot() {
    BleSnapshot snap{};
    snap.live = g_live;
    snap.vibration_mg = s_vibrationMg;
    snap.ina_ok = g_current.isHealthy();
    snap.tmp_ok = g_temp.ambientHealthy();
    return snap;
}

void printAllSensors() {
    printSensorReport(g_live, s_vibrationMg,
                      g_current.isHealthy(), g_temp.ambientHealthy());
}

bool isMonitorAutoEnabled() { return s_monitorAuto; }
void setMonitorAuto(bool on) { s_monitorAuto = on; s_lastMonitorMs = 0; }

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
    g_psram.pushMeasurement(rec);
    g_logger.logMeasurement(rec);
}

void setup() {
    pinMode(LED_GREEN_PIN, OUTPUT);
    digitalWrite(LED_GREEN_PIN, HIGH);

    Serial.begin(115200);
    delay(500);
    Serial.println("Ventec AGV Monitor v1.3 - demarrage");

    g_time.begin();

    const bool psramOk = g_psram.begin();

    g_i2c.begin();
    g_lab.begin();
    g_power.begin();
    g_current.begin();
    g_temp.begin();
    g_logger.begin();
    if (!psramOk) {
        Serial.println("ATTENTION: PSRAM indisponible — historique RAM interne seulement");
    }
    g_alarm.begin();
    g_indicators.begin();
    g_ble.begin();

    logBootEvent();

    if (!psramOk) {
        SystemEventRecord ev{};
        ev.timestamp_ms = g_time.nowMs();
        ev.type = SystemEventType::SENSOR_FAULT;
        ev.param = 2;
        g_logger.logSystemEvent(ev);
    }

    if (!g_current.isHealthy()) {
        SystemEventRecord ev{};
        ev.timestamp_ms = g_time.nowMs();
        ev.type = SystemEventType::SENSOR_FAULT;
        ev.param = 1;
        g_logger.logSystemEvent(ev);
        Serial.println("ATTENTION: INA237 non detecte (verifier I2C)");
    }

    Serial.println("Pret. Tapez HELP pour les commandes.");
    Serial.println("Astuce test sans AGV : LABON puis MONITOR ON");
}

void loop() {
    uint32_t now = millis();

    handlePowerChange();
    g_cli.process(g_live);

    if (now - s_lastMeasureMs >= MEASURE_INTERVAL_MS) {
        s_lastMeasureMs = now;
        acquireMeasurements();
        g_ble.updateSnapshot(buildBleSnapshot());
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

    if (s_monitorAuto && (now - s_lastMonitorMs >= 1000)) {
        s_lastMonitorMs = now;
        printAllSensors();
    }
}
