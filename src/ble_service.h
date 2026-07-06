#pragma once

#include "ble_snapshot.h"
#include "types.h"

class BleService {
public:
    bool begin();
    void updateSnapshot(const BleSnapshot& snap);
    void notifyIfConnected();
    void notifyAlarmEvent(const AlarmRecord& rec);
    bool isConnected() const { return _connected; }
    void setConnected(bool c) { _connected = c; }

private:
    void setupGatt();
    void notifyLiveCharacteristics();
    void notifyTelemetry();

    bool         _connected = false;
    BleSnapshot  _snap{};
    AlarmRecord  _lastAlarmNotify{};

    friend class ServerCallbacks;
};

extern BleService g_ble;
