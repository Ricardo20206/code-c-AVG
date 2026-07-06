#pragma once

#include "types.h"

// Données live étendues envoyées au client BLE (tablette technicien)
struct BleSnapshot {
    LiveData live{};
    float    vibration_mg = -1.0f;
    bool     ina_ok       = false;
    bool     tmp_ok       = false;
};
