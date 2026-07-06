#include "lab_test.h"
#include <Arduino.h>

LabTest g_lab;

bool LabTest::begin() {
    return true;
}

void LabTest::enable(bool on) {
    _active = on;
    if (on) {
        Serial.println("=== MODE LABO ACTIF (EXF-10) ===");
        Serial.println("Capteurs simules : courant, 3 temperatures, humidite, vibration");
        printProcedure();
    } else {
        Serial.println("Mode labo desactive");
    }
}

void LabTest::setSimulatedCurrent(float amps)       { _simCurrent = amps; }
void LabTest::setSimulatedTempPcb1(float c)        { _simTempPcb1 = c; }
void LabTest::setSimulatedTempPcb2(float c)        { _simTempPcb2 = c; }
void LabTest::setSimulatedTempAmbient(float c)     { _simTempAmb = c; }
void LabTest::setSimulatedHumidity(float pct)      { _simHumidity = pct; }
void LabTest::setSimulatedVibrationMg(float mg)    { _simVibrationMg = mg; }

void LabTest::applyTo(LiveData& data) const {
    if (!_active) {
        data.lab_active = false;
        return;
    }
    data.lab_active          = true;
    data.current_a           = _simCurrent;
    data.temp_pcb1_c         = _simTempPcb1;
    data.temp_pcb2_c         = _simTempPcb2;
    data.temp_ambient_c      = _simTempAmb;
    data.humidity_pct        = _simHumidity;
    data.vibration_present   = true;
    data.humidity_present    = true;
    data.lora_present        = false;
    data.power_source        = PowerSource::HT_36V;
}

void LabTest::printProcedure() const {
    Serial.println("Commandes simulation :");
    Serial.println("  SENSORS         Afficher tous les capteurs");
    Serial.println("  MONITOR ON      Affichage continu (1 s)");
    Serial.println("  MONITOR OFF     Arreter affichage continu");
    Serial.println("  SIM <A>         Courant simule");
    Serial.println("  SIMTEMP <C>     Temperature PCB1");
    Serial.println("  SIMTEMP2 <C>    Temperature PCB2");
    Serial.println("  SIMAMB <C>      Temperature ambiante");
    Serial.println("  SIMHUM <pct>    Humidite");
    Serial.println("  SIMVIB <mg>     Vibration");
    Serial.println("  LABOFF          Quitter mode labo");
}
