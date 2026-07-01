#pragma once

// Calibration courant (EXF-10) — offset et gain stockés en NVS
class CurrentCalibration {
public:
    bool begin();
    float apply(float rawAmps) const;
    float offset() const { return _offset; }
    float gain()   const { return _gain; }

    // Calibrer avec une valeur de référence connue (ampèremètre labo)
    bool calibrateWithReference(float measuredRaw, float referenceA);
    void reset();
    void save();

private:
    void load();

    float _offset = 0.0f;
    float _gain   = 1.0f;
};

extern CurrentCalibration g_calib;
