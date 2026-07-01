#pragma once

#include "types.h"

// Mode laboratoire (EXF-10) — validation sans AGV connecté
class LabTest {
public:
    bool isActive() const { return _active; }
    void enable(bool on);
    void setSimulatedCurrent(float amps);
    void setSimulatedTempPcb1(float c);
    void setSimulatedTempPcb2(float c);
    void applyTo(LiveData& data) const;
    void printProcedure() const;

private:
    bool  _active = false;
    float _simCurrent  = 5.0f;
    float _simTempPcb1 = 35.0f;
    float _simTempPcb2 = 38.0f;
    float _simTempAmb  = 25.0f;
};

extern LabTest g_lab;
