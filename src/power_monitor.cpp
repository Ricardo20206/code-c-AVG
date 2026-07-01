#include "power_monitor.h"
#include "board_config.h"
#include <Arduino.h>

PowerMonitor g_power;

bool PowerMonitor::begin() {
    pinMode(PWR_DETECT_USB_PIN, INPUT);
    pinMode(PWR_DETECT_BT_PIN, INPUT);
    pinMode(PWR_DETECT_HT_PIN, INPUT);
    _lastSource = readSource();
    update();
    return true;
}

void PowerMonitor::update() {
    const PowerSource source = readSource();
    switch (source) {
        case PowerSource::USB_C:
            _batteryVoltage = 5.0f;
            _batteryPercent = 100.0f;
            break;
        case PowerSource::BT_SVC:
            _batteryVoltage = 12.0f;
            _batteryPercent = 80.0f;
            break;
        case PowerSource::HT_36V:
        default:
            _batteryVoltage = 36.0f;
            _batteryPercent = 92.0f;
            break;
    }
}

float PowerMonitor::readBatteryPercent() {
    update();
    return _batteryPercent;
}

float PowerMonitor::readBatteryVoltage() {
    update();
    return _batteryVoltage;
}

PowerSource PowerMonitor::readSource() const {
    // Priorité matérielle : USB > BT > HT (EXF-03)
    if (digitalRead(PWR_DETECT_USB_PIN) == HIGH) return PowerSource::USB_C;
    if (digitalRead(PWR_DETECT_BT_PIN) == HIGH) return PowerSource::BT_SVC;
    if (digitalRead(PWR_DETECT_HT_PIN) == HIGH) return PowerSource::HT_36V;
    return PowerSource::HT_36V; // défaut exploitation AGV
}

const char* PowerMonitor::sourceName(PowerSource src) const {
    switch (src) {
        case PowerSource::USB_C:  return "USB-C";
        case PowerSource::BT_SVC: return "BT-Service";
        case PowerSource::HT_36V: return "HT-36V";
        default:                  return "Inconnu";
    }
}

bool PowerMonitor::hasSourceChanged() {
    PowerSource current = readSource();
    if (current != _lastSource) {
        _lastSource = current;
        return true;
    }
    return false;
}
