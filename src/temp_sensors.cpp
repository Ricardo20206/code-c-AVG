#include "temp_sensors.h"
#include "board_config.h"
#include <Arduino.h>
#include <TMP126.h>
#include <cmath>

TempSensors g_temp;
static TMP126 tmp126(TMP126_CS_PIN, TMP126_MOSI_PIN, TMP126_MISO_PIN, TMP126_SCLK_PIN);

bool TempSensors::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    tmp126.begin();
    _ambientOk = tmp126.getStatus();
    return true;
}

float TempSensors::ntcAdcToCelsius(int adcPin) {
    int raw = analogRead(adcPin);
    if (raw <= 0) return -40.0f;

    float voltage = (float)raw / 4095.0f * 3.3f;
    float rNTC = NTC_SERIES_RES_OHM * voltage / (3.3f - voltage);
    if (rNTC <= 0.0f) return -40.0f;

    float steinhart = rNTC / NTC_NOMINAL_OHM;
    steinhart = log(steinhart);
    steinhart /= NTC_BETA;
    steinhart += 1.0f / (NTC_NOMINAL_TEMP_C + 273.15f);
    steinhart = 1.0f / steinhart;
    return steinhart - 273.15f;
}

float TempSensors::readPcb1C() { return ntcAdcToCelsius(NTC_PCB1_ADC_PIN); }
float TempSensors::readPcb2C() { return ntcAdcToCelsius(NTC_PCB2_ADC_PIN); }

float TempSensors::readAmbientC() {
    if (!_ambientOk) return -999.0f;
    return tmp126.getTemp();
}
