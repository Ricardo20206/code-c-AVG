#include "lab_test.h"
#include <Arduino.h>

LabTest g_lab;

void LabTest::enable(bool on) {
    _active = on;
    if (on) {
        Serial.println("=== MODE LABO ACTIF (EXF-10) ===");
        printProcedure();
    } else {
        Serial.println("Mode labo desactive");
    }
}

void LabTest::setSimulatedCurrent(float amps)  { _simCurrent = amps; }
void LabTest::setSimulatedTempPcb1(float c)    { _simTempPcb1 = c; }
void LabTest::setSimulatedTempPcb2(float c)    { _simTempPcb2 = c; }

void LabTest::applyTo(LiveData& data) const {
    if (!_active) return;
    data.current_a      = _simCurrent;
    data.temp_pcb1_c    = _simTempPcb1;
    data.temp_pcb2_c    = _simTempPcb2;
    data.temp_ambient_c = _simTempAmb;
}

void LabTest::printProcedure() const {
    Serial.println("Procedure validation laboratoire :");
    Serial.println("  1. Connecter alimentation 36V simulee via resistances de charge");
    Serial.println("  2. Points de test TP_SHUNT+ / TP_SHUNT- : verifier tension shunt");
    Serial.println("  3. Commande CAL <ref_A> : calibrer avec ampèremetre de reference");
    Serial.println("  4. Commande RAW : afficher valeurs brutes INA219");
    Serial.println("  5. Commande SIM <A> : simuler un courant (sans AGV)");
    Serial.println("  6. Commande I2CSCAN : verifier bus capteurs");
    Serial.println("  7. Commande LABOFF : quitter le mode labo");
}
