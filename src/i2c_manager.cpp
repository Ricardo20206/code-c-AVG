#include "i2c_manager.h"
#include "board_config.h"
#include <Adafruit_LIS3DH.h>
#include <Adafruit_SHT31.h>
#include <Wire.h>
#include <cmath>

I2cManager g_i2c;
static Adafruit_LIS3DH lis3dh;
static Adafruit_SHT31  sht31;

bool I2cManager::probeAddress(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool I2cManager::begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
    scanBus();
    return true;
}

void I2cManager::scanBus() {
    _vibrationOk = lis3dh.begin(GROVE1_I2C_ADDR_VIB);
    if (!_vibrationOk) {
        _vibrationOk = probeAddress(GROVE1_I2C_ADDR_VIB);
    }

    _humidityOk = sht31.begin(GROVE2_I2C_ADDR_HUM);
    if (!_humidityOk) {
        _humidityOk = probeAddress(GROVE2_I2C_ADDR_HUM);
    }

    // LoRa SX1276 : détection via SPI CS (GPIO 5 MikroBus typique) - placeholder
    _loraPresent = false;
}

float I2cManager::readVibrationMg() {
    if (!_vibrationOk) return -1.0f;
    sensors_event_t event;
    lis3dh.getEvent(&event);
    float mag = sqrtf(event.acceleration.x * event.acceleration.x +
                      event.acceleration.y * event.acceleration.y +
                      event.acceleration.z * event.acceleration.z);
    return mag * 1000.0f; // en mg approximatif
}

float I2cManager::readHumidityPct() {
    if (!_humidityOk) return -1.0f;
    return sht31.readHumidity();
}
