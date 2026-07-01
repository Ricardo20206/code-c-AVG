
#include "indicators.h"
#include "board_config.h"
#include <Arduino.h>

Indicators g_indicators;

bool Indicators::begin() {
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQ, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
    setGreen(true);  // EXF-24 : LED verte permanente en fonctionnement normal
    setRed(false);
    setBuzzer(false);
    return true;
}

void Indicators::setGreen(bool on) { digitalWrite(LED_GREEN_PIN, on ? HIGH : LOW); }
void Indicators::setRed(bool on)   { digitalWrite(LED_RED_PIN, on ? HIGH : LOW); }

void Indicators::setBuzzer(bool on) {
    ledcWrite(BUZZER_PWM_CHANNEL, on ? 128 : 0);
}

void Indicators::playTone(uint16_t freqHz, uint16_t durationMs) {
    ledcWriteTone(BUZZER_PWM_CHANNEL, freqHz);
    delay(durationMs);
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
}

void Indicators::playPattern(AlarmLevel level) {
    // EXF-27 : motifs distincts avertissement / critique
    if (level == AlarmLevel::WARNING) {
        playTone(1500, 200);
        delay(100);
        playTone(1500, 200);
    } else if (level == AlarmLevel::CRITICAL) {
        for (int i = 0; i < 3; ++i) {
            playTone(2500, 300);
            delay(150);
        }
    }
}

void Indicators::update(AlarmLevel level) {
    switch (level) {
        case AlarmLevel::NONE:
            setGreen(true);
            setRed(false);
            setBuzzer(false);
            break;

        case AlarmLevel::WARNING:
            setGreen(false);
            _blinkState = !_blinkState;
            setRed(_blinkState);
            setBuzzer(_blinkState);
            break;

        case AlarmLevel::CRITICAL:
            // EXF-25 : LED rouge + buzzer simultanés
            setGreen(false);
            setRed(true);
            setBuzzer(true);
            break;
    }

    if (level != _lastLevel && level != AlarmLevel::NONE) {
        playPattern(level);
    }
    _lastLevel = level;
}
