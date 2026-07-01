#pragma once

#include "types.h"

class BleService {
public:
    bool begin();
    void updateLiveData(const LiveData& data);
    void notifyIfConnected();
    bool isConnected() const { return _connected; }
    void setConnected(bool c) { _connected = c; }

    static void onThresholdsWritten(const AlarmThresholds& t);

private:
    void setupGatt();
    void notifyCharacteristics();

    bool     _connected = false;
    LiveData _live{};

    friend class ServerCallbacks;
};

extern BleService g_ble;
