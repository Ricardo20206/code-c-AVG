#include "ble_service.h"
#include "alarm_manager.h"
#include "ble_data.h"
#include "board_config.h"
#include "data_logger.h"
#include "time_service.h"
#include <NimBLEDevice.h>
#include <cstring>

BleService g_ble;

// UUIDs Ventec Monitoring GATT (EXF-19) — base Nordic UART, suffixes Ventec
static const char* SVC_UUID         = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_CURRENT     = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_TEMPS       = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_ALARM       = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_THRESH      = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_LOGS        = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_STATUS      = "6e400007-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_TELEMETRY   = "6e400008-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_ALARM_EVENT = "6e400009-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_ALARM_HIST  = "6e40000a-b5a3-f393-e0a9-e50e24dcca9e";

static NimBLECharacteristic* s_charCurrent     = nullptr;
static NimBLECharacteristic* s_charTemps         = nullptr;
static NimBLECharacteristic* s_charAlarm       = nullptr;
static NimBLECharacteristic* s_charThresh        = nullptr;
static NimBLECharacteristic* s_charLogs          = nullptr;
static NimBLECharacteristic* s_charStatus        = nullptr;
static NimBLECharacteristic* s_charTelemetry   = nullptr;
static NimBLECharacteristic* s_charAlarmEvent  = nullptr;
static NimBLECharacteristic* s_charAlarmHist     = nullptr;

#pragma pack(push, 1)
struct BleAlarmEventPayload {
    uint32_t timestamp_ms;
    uint8_t  type;
    uint8_t  level;
    float    trigger_value;
    float    threshold;
};
struct BleTelemetryPayload {
    float   humidity_pct;
    float   vibration_mg;
    uint8_t sensor_flags;
    uint8_t mode_flags;
};
#pragma pack(pop)

static uint8_t buildSensorFlags(const BleSnapshot& snap) {
    uint8_t f = 0;
    if (snap.ina_ok) f |= 0x01;
    if (snap.tmp_ok) f |= 0x02;
    if (snap.live.vibration_present) f |= 0x04;
    if (snap.live.humidity_present) f |= 0x08;
    if (snap.live.lora_present) f |= 0x10;
    return f;
}

static uint8_t buildModeFlags(const BleSnapshot& snap) {
    uint8_t f = 0;
    if (snap.live.lab_active) f |= 0x01;
    return f;
}

static void packAlarmEvent(const AlarmRecord& rec, BleAlarmEventPayload& out) {
    out.timestamp_ms  = rec.timestamp_ms;
    out.type          = (uint8_t)rec.type;
    out.level         = (uint8_t)rec.level;
    out.trigger_value = rec.trigger_value;
    out.threshold     = rec.threshold;
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        g_ble.setConnected(true);
        SystemEventRecord ev{};
        ev.timestamp_ms = g_time.nowMs();
        ev.type = SystemEventType::TECH_BLE_CONN;
        g_logger.logSystemEvent(ev);
        g_ble.notifyIfConnected();
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
            pChar->setValue((uint8_t*)&t, sizeof(t));
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

class AlarmHistCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pChar) override {
        static AlarmRecord buf[50];
        size_t count = ventecGetAlarmHistory(buf, 50);
        uint8_t header[1] = {(uint8_t)(count > 255 ? 255 : count)};
        std::string payload;
        payload.reserve(1 + count * sizeof(AlarmRecord));
        payload.append((char*)header, 1);
        if (count > 0) {
            payload.append((char*)buf, count * sizeof(AlarmRecord));
        }
        pChar->setValue((uint8_t*)payload.data(), payload.size());
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
    s_charTelemetry = service->createCharacteristic(
        CHAR_TELEMETRY, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_charAlarmEvent = service->createCharacteristic(
        CHAR_ALARM_EVENT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    s_charThresh = service->createCharacteristic(
        CHAR_THRESH, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    s_charThresh->setCallbacks(new ThresholdCallbacks());
    AlarmThresholds t = g_alarm.thresholds();
    s_charThresh->setValue((uint8_t*)&t, sizeof(t));

    s_charLogs = service->createCharacteristic(CHAR_LOGS, NIMBLE_PROPERTY::READ);
    s_charLogs->setCallbacks(new LogsCallbacks());

    s_charAlarmHist = service->createCharacteristic(CHAR_ALARM_HIST, NIMBLE_PROPERTY::READ);
    s_charAlarmHist->setCallbacks(new AlarmHistCallbacks());

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advData;
    advData.setName(BLE_DEVICE_NAME);
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    NimBLEAdvertisementData scanData;
    scanData.setName(BLE_DEVICE_NAME);
    adv->setAdvertisementData(advData);
    adv->setScanResponseData(scanData);
    adv->addServiceUUID(SVC_UUID);
    adv->start();
}

bool BleService::begin() {
    setupGatt();
    return true;
}

void BleService::updateSnapshot(const BleSnapshot& snap) {
    _snap = snap;
}

void BleService::notifyLiveCharacteristics() {
    if (!_connected) return;

    const LiveData& live = _snap.live;

    float cur = live.current_a;
    s_charCurrent->setValue((uint8_t*)&cur, sizeof(cur));
    s_charCurrent->notify();

    float temps[3] = {live.temp_pcb1_c, live.temp_pcb2_c, live.temp_ambient_c};
    s_charTemps->setValue((uint8_t*)temps, sizeof(temps));
    s_charTemps->notify();

    uint8_t alarmByte = (uint8_t)live.alarm_level;
    s_charAlarm->setValue(&alarmByte, 1);
    s_charAlarm->notify();

    uint8_t status[4] = {
        (uint8_t)live.power_source,
        (uint8_t)(live.vibration_present ? 1 : 0),
        (uint8_t)(live.humidity_present ? 1 : 0),
        (uint8_t)(live.lora_present ? 1 : 0),
    };
    s_charStatus->setValue(status, sizeof(status));
    s_charStatus->notify();

    notifyTelemetry();
}

void BleService::notifyTelemetry() {
    if (!_connected || !s_charTelemetry) return;

    BleTelemetryPayload payload{};
    payload.humidity_pct  = _snap.live.humidity_pct;
    payload.vibration_mg  = _snap.vibration_mg;
    payload.sensor_flags  = buildSensorFlags(_snap);
    payload.mode_flags    = buildModeFlags(_snap);
    s_charTelemetry->setValue((uint8_t*)&payload, sizeof(payload));
    s_charTelemetry->notify();
}

void BleService::notifyAlarmEvent(const AlarmRecord& rec) {
    _lastAlarmNotify = rec;
    if (!s_charAlarmEvent) return;

    BleAlarmEventPayload payload{};
    packAlarmEvent(rec, payload);
    s_charAlarmEvent->setValue((uint8_t*)&payload, sizeof(payload));

    if (_connected) {
        s_charAlarmEvent->notify();
        uint8_t alarmByte = (uint8_t)rec.level;
        s_charAlarm->setValue(&alarmByte, 1);
        s_charAlarm->notify();
    }
}

void BleService::notifyIfConnected() {
    notifyLiveCharacteristics();
}
