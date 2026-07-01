/**
 * @file main.cpp
 * @brief Systeme de monitoring AGV - version modulaire
 */

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "types.h"
#include "alarm_manager.h"
#include "ble_service.h"
#include "board_config.h"
#include "calibration.h"
#include "current_sensor.h"
#include "data_logger.h"
#include "i2c_manager.h"
#include "indicators.h"
#include "power_monitor.h"
#include "serial_cli.h"
#include "temp_sensors.h"
#include "time_service.h"

namespace {
LiveData g_live{};
uint32_t g_lastMeasureMs = 0;
uint32_t g_lastLogMs = 0;
uint32_t g_lastBleMs = 0;
uint32_t g_lastIndicatorMs = 0;
}

void onAlarmTriggered(const AlarmRecord& rec) {
    g_logger.logAlarm(rec);
}

namespace {

void acquireMeasurements() {
    g_live.current_a = g_current.readCurrentA();
    g_live.temp_pcb1_c = g_temp.readPcb1C();
    g_live.temp_pcb2_c = g_temp.readPcb2C();
    g_live.temp_ambient_c = g_temp.readAmbientC();
    g_live.humidity_pct = g_i2c.readHumidityPct();
    g_live.power_source = g_power.readSource();
    g_live.vibration_present = g_i2c.vibrationPresent();
    g_live.humidity_present = g_i2c.humidityPresent();
    g_live.lora_present = g_i2c.loraPresent();

    g_alarm.update(g_live.current_a, g_live.temp_pcb1_c, g_live.temp_pcb2_c);
    g_live.alarm_level = g_alarm.level();
}

void persistMeasurement() {
    MeasurementRecord rec{};
    rec.timestamp_ms = g_time.nowMs();
    rec.current_a = g_live.current_a;
    rec.temp_pcb1_c = g_live.temp_pcb1_c;
    rec.temp_pcb2_c = g_live.temp_pcb2_c;
    rec.temp_ambient_c = g_live.temp_ambient_c;
    rec.humidity_pct = g_live.humidity_pct;
    rec.alarm_level = (uint8_t)g_live.alarm_level;
    rec.power_source = (uint8_t)g_live.power_source;
    g_logger.logMeasurement(rec);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================");
    Serial.println("SYSTEME DE MONITORING AGV");
    Serial.println("Version modulaire");
    Serial.println("========================================\n");

    bool ok = true;

    if (!g_time.begin()) {
        Serial.println("[System] Echec TimeService");
        ok = false;
    }

    if (!g_i2c.begin()) {
        Serial.println("[System] Echec I2C");
        ok = false;
    }

    if (!g_power.begin()) {
        Serial.println("[System] Echec PowerMonitor");
        ok = false;
    }

    if (!g_current.begin()) {
        Serial.println("[System] CurrentSensor non detecte");
    }

    if (!g_temp.begin()) {
        Serial.println("[System] TempSensors non detecte");
    }

    if (!g_logger.begin()) {
        Serial.println("[System] Echec DataLogger");
        ok = false;
    }

    if (!g_alarm.begin()) {
        Serial.println("[System] Echec AlarmManager");
        ok = false;
    }

    if (!g_indicators.begin()) {
        Serial.println("[System] Indicators non initialise");
        ok = false;
    }

    if (!g_ble.begin()) {
        Serial.println("[System] BLEService non initialise");
        ok = false;
    }

    if (!g_calib.begin()) {
        Serial.println("[System] Calibration non initialise");
        ok = false;
    }

    SystemEventRecord bootEv{};
    bootEv.timestamp_ms = g_time.nowMs();
    bootEv.type = SystemEventType::BOOT;
    g_logger.logSystemEvent(bootEv);

    if (!g_current.isHealthy()) {
        SystemEventRecord fault{};
        fault.timestamp_ms = g_time.nowMs();
        fault.type = SystemEventType::SENSOR_FAULT;
        fault.param = 1;
        g_logger.logSystemEvent(fault);
        Serial.println("ATTENTION: INA219 non detecte (verifier I2C)");
    }

    if (ok) {
        Serial.println("Systeme operationnel");
        Serial.println("Scan BLE pour se connecter");
        Serial.println("CLI via USB (help pour les commandes)\n");
    } else {
        Serial.println("Echec de l'initialisation");
    }
}

void loop() {
    const uint32_t now = millis();

    g_cli.process(g_live);

    if (g_power.hasSourceChanged()) {
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

    if (now - g_lastMeasureMs >= MEASURE_INTERVAL_MS) {
        g_lastMeasureMs = now;
        acquireMeasurements();
        g_ble.updateLiveData(g_live);
    }

    if (now - g_lastLogMs >= LOG_INTERVAL_MS) {
        g_lastLogMs = now;
        persistMeasurement();
    }

    if (now - g_lastBleMs >= BLE_NOTIFY_INTERVAL_MS) {
        g_lastBleMs = now;
        g_ble.notifyIfConnected();
    }

    if (now - g_lastIndicatorMs >= 500) {
        g_lastIndicatorMs = now;
        g_indicators.update(g_live.alarm_level);
    }

    delay(10);
}