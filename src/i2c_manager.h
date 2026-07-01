#pragma once

#include "types.h"

// Gestion des capteurs I2C extensibles (Grove 1/2, MikroBus)
class I2cManager {
public:
    bool begin();
    void scanBus();
    bool vibrationPresent() const { return _vibrationOk; }
    bool humidityPresent()  const { return _humidityOk; }
    bool loraPresent()      const { return _loraPresent; }

    float readVibrationMg();   // magnitude accélération
    float readHumidityPct();   // -1 si absent

private:
    bool probeAddress(uint8_t addr);
    bool _vibrationOk = false;
    bool _humidityOk  = false;
    bool _loraPresent = false;
};

extern I2cManager g_i2c;
