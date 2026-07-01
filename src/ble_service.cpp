#include "ble_service.h"
#include "alarm_manager.h"
#include "board_config.h"
#include "data_logger.h"
#include "time_service.h"
#include <NimBLEDevice.h>
#include <cstring>

BleService g_ble;

// UUIDs Ventec Monitoring GATT (EXF-19)
static const char* SVC_UUID       = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_CURRENT   = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_TEMPS     = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_ALARM     = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_THRESH    = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_LOGS      = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_STATUS    = "6e400007-b5a3-f393-e0a9-e50e24dcca9e";

static NimBLECharacteristic* s_charCurrent = nullptr;
static NimBLECharacteristic* s_charTemps     = nullptr;
static NimBLECharacteristic* s_charAlarm   = nullptr;
static NimBLECharacteristic* s_charThresh    = nullptr;
static NimBLECharacteristic* s_charLogs     = nullptr;
static NimBLECharacteristic* s_charStatus   = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        g_ble.setConnected(true);
        SystemEventRecord ev{};
        ev.timestamp_ms = g_time.nowMs();
        ev.type = SystemEventType::TECH_BLE_CONN;
        g_logger.logSystemEvent(ev);
    }
    void onDisconnect(NimBLEServer* pServer) override {
        g_ble.setConnected(false);
        NimBLEDevice::startAdvertising();
    }
};

class ThresholdCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (val.size() < sizeof(AlarmThresholds)) return;

        AlarmThresholds t{};
        memcpy(&t, val.data(), sizeof(AlarmThresholds));
        if (g_alarm.setThresholds(t)) {
            SystemEventRecord ev{};
            ev.timestamp_ms = g_time.nowMs();
            ev.type = SystemEventType::THRESHOLD_CHANGE;
            g_logger.logSystemEvent(ev);
        }
    }
};

class LogsCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pChar) override {
        String csv = g_logger.exportAllCsv();
        pChar->setValue((uint8_t*)csv.c_str(), csv.length());
    }
};

void BleService::setupGatt() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* service = server->createService(SVC_UUID);

    s_charCurrent = service->createCharacteristic(
        CHAR_CURRENT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_charTemps = service->createCharacteristic(
        CHAR_TEMPS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_charAlarm = service->createCharacteristic(
        CHAR_ALARM, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_charStatus = service->createCharacteristic(
        CHAR_STATUS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    // EXF-23 : seuils modifiables via BLE
    s_charThresh = service->createCharacteristic(
        CHAR_THRESH,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    s_charThresh->setCallbacks(new ThresholdCallbacks());
    AlarmThresholds t = g_alarm.thresholds();
    s_charThresh->setValue((uint8_t*)&t, sizeof(t));

    s_charLogs = service->createCharacteristic(
        CHAR_LOGS, NIMBLE_PROPERTY::READ);
    s_charLogs->setCallbacks(new LogsCallbacks());

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->start();
}

bool BleService::begin() {
    setupGatt();
    return true;
}

void BleService::updateLiveData(const LiveData& data) {
    _live = data;
}

void BleService::notifyCharacteristics() {
    if (!_connected) return;

    float cur = _live.current_a;
    s_charCurrent->setValue((uint8_t*)&cur, sizeof(cur));
    s_charCurrent->notify();

    float temps[3] = {_live.temp_pcb1_c, _live.temp_pcb2_c, _live.temp_ambient_c};
    s_charTemps->setValue((uint8_t*)temps, sizeof(temps));
    s_charTemps->notify();

    uint8_t alarmByte = (uint8_t)_live.alarm_level;
    s_charAlarm->setValue(&alarmByte, 1);
    s_charAlarm->notify();

    uint8_t status[4] = {
        (uint8_t)_live.power_source,
        (uint8_t)(_live.vibration_present ? 1 : 0),
        (uint8_t)(_live.humidity_present ? 1 : 0),
        (uint8_t)(_live.lora_present ? 1 : 0)
    };
    s_charStatus->setValue(status, sizeof(status));
    s_charStatus->notify();
}

void BleService::notifyIfConnected() {
    notifyCharacteristics();
}

void BleService::onThresholdsWritten(const AlarmThresholds& t) {
    g_alarm.setThresholds(t);
}
