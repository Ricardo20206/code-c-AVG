#include "serial_cli.h"
#include "alarm_manager.h"
#include "board_config.h"
#include "calibration.h"
#include "current_sensor.h"
#include "data_logger.h"
#include "i2c_manager.h"
#include "indicators.h"
#include "lab_test.h"
#include "power_monitor.h"
#include <Arduino.h>
#include <Wire.h>

SerialCli g_cli;

static String s_cmdBuffer;

static String normalizeCommand(String s) {
    s.trim();
    s.toUpperCase();
    return s;
}

// Lit une ligne complète (gère \r, \n, caractères parasites Windows)
static bool pollCommandLine(String& out) {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_cmdBuffer.length() == 0) continue;
            out = normalizeCommand(s_cmdBuffer);
            s_cmdBuffer = "";
            return true;
        }
        if (c >= 32 && c <= 126) {
            s_cmdBuffer += c;
            if (s_cmdBuffer.length() > 64) s_cmdBuffer = "";
        }
    }
    return false;
}

static void printHelp() {
    Serial.println("--- Commandes Ventec AGV Monitor ---");
    Serial.println("  STATUS          Etat instantane");
    Serial.println("  EXPORT / LOGS   Export CSV complet (EXF-21)");
    Serial.println("  CLEAR           Effacer les logs");
    Serial.println("  I2CSCAN         Scanner le bus I2C (EXF-31)");
    Serial.println("  RAW             Valeurs brutes capteur courant");
    Serial.println("  CAL <A>         Calibrer avec courant reference (EXF-10)");
    Serial.println("  CALRESET        Reinitialiser calibration");
    Serial.println("  LABON           Activer mode laboratoire");
    Serial.println("  LABOFF          Desactiver mode laboratoire");
    Serial.println("  SIM <A>         Simuler courant (mode labo)");
    Serial.println("  SIMTEMP <C>     Simuler T° PCB1 (mode labo)");
    Serial.println("  THRESH          Afficher seuils alarme");
    Serial.println("  THRESH <w> <a> <t>  Modifier seuils (A, A, °C)");
    Serial.println("  GPIO            Etat broches detection alimentation");
    Serial.println("  STORAGE         Statistiques stockage (Jalon 1)");
    Serial.println("  BUZZER          Bip test (GPIO " + String(BUZZER_PIN) + ")");
    Serial.println("  BUZZER WARN     Motif sonore avertissement");
    Serial.println("  BUZZER ALARM    Motif sonore alarme critique");
    Serial.println("  BUZZER <Hz> <ms>  Bip personnalise (ex: BUZZER 2500 300)");
    Serial.println("  VERSION         Version firmware");
    Serial.println("  HELP            Cette aide");
}

static void i2cScan() {
    Serial.println("Scan I2C (SDA=" + String(I2C_SDA_PIN) + " SCL=" + String(I2C_SCL_PIN) + ")");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X detecte\n", addr);
            found++;
        }
    }
    if (found == 0) Serial.println("  Aucun peripherique detecte");
}

static void printStorageStats() {
  size_t measSize = sizeof(MeasurementRecord) * LOG_MAX_MEASUREMENTS;
  size_t alarmSize = sizeof(AlarmRecord) * LOG_MAX_ALARMS;
  size_t sysSize   = sizeof(SystemEventRecord) * LOG_MAX_SYSTEM_EVENTS;
  size_t total     = measSize + alarmSize + sysSize;

  Serial.println("--- Architecture stockage (Jalon 1) ---");
  Serial.printf("  Mesures  : %u entrees x %u octets = %u Ko\n",
                LOG_MAX_MEASUREMENTS, sizeof(MeasurementRecord), measSize / 1024);
  Serial.printf("  Alarmes  : %u entrees x %u octets = %u Ko\n",
                LOG_MAX_ALARMS, sizeof(AlarmRecord), alarmSize / 1024);
  Serial.printf("  Systeme  : %u entrees x %u octets = %u Ko\n",
                LOG_MAX_SYSTEM_EVENTS, sizeof(SystemEventRecord), sysSize / 1024);
  Serial.printf("  Total max: %u Ko (rotation EXF-18)\n", total / 1024);
  Serial.printf("  Enregistrees: meas=%u alarm=%u sys=%u\n",
                g_logger.measurementCount(), g_logger.alarmCount(), g_logger.systemCount());
}

void SerialCli::process(const LiveData& live) {
    String cmd;
    if (!pollCommandLine(cmd)) return;
    if (cmd.length() == 0) return;

  if (cmd == "EXPORT" || cmd == "LOGS") {
    Serial.println(g_logger.exportAllCsv());
  } else if (cmd == "STATUS") {
    Serial.printf("Courant : %.2f A (cal offset=%.3f gain=%.3f)\n",
                  live.current_a, g_calib.offset(), g_calib.gain());
    Serial.printf("T° PCB1 : %.1f °C | PCB2 : %.1f °C | Amb : %.1f °C\n",
                  live.temp_pcb1_c, live.temp_pcb2_c, live.temp_ambient_c);
    Serial.printf("Humidite: %.1f %% | Alarme: %d\n",
                  live.humidity_pct, (int)live.alarm_level);
    Serial.printf("Source  : %s | BLE: %s | Labo: %s\n",
                  g_power.sourceName(live.power_source),
                  "voir module BLE", g_lab.isActive() ? "OUI" : "NON");
  } else if (cmd == "CLEAR") {
    g_logger.clearAll();
    Serial.println("OK: logs effaces");
  } else if (cmd == "HELP") {
    printHelp();
  } else if (cmd == "I2CSCAN") {
    i2cScan();
  } else if (cmd == "RAW") {
    float raw = g_current.readRawAmps();
    Serial.printf("Brut INA219: %.4f A | Apres cal: %.4f A\n",
                  raw, g_calib.apply(raw));
  } else if (cmd.startsWith("CAL ")) {
    float ref = cmd.substring(4).toFloat();
    float raw = g_current.readRawAmps();
    if (g_calib.calibrateWithReference(raw, ref)) {
      Serial.printf("OK: calibre ref=%.2f A (raw=%.4f, gain=%.4f)\n",
                    ref, raw, g_calib.gain());
    } else {
      Serial.println("ERREUR: calibration invalide");
    }
  } else if (cmd == "CALRESET") {
    g_calib.reset();
    Serial.println("OK: calibration reinitialisee");
  } else if (cmd == "LABON") {
    g_lab.enable(true);
  } else if (cmd == "LABOFF") {
    g_lab.enable(false);
  } else if (cmd.startsWith("SIM ")) {
    float a = cmd.substring(4).toFloat();
    g_lab.setSimulatedCurrent(a);
    Serial.printf("Simulation courant: %.2f A\n", a);
  } else if (cmd.startsWith("SIMTEMP ")) {
    float t = cmd.substring(8).toFloat();
    g_lab.setSimulatedTempPcb1(t);
    Serial.printf("Simulation T° PCB1: %.1f °C\n", t);
  } else if (cmd == "THRESH") {
    auto t = g_alarm.thresholds();
    Serial.printf("Warn=%.1f A | Alarm=%.1f A | Temp=%.0f °C | Drift=%.2f A\n",
                  t.current_warn_a, t.current_alarm_a, t.temp_alarm_c, t.current_drift_a);
  } else if (cmd.startsWith("THRESH ")) {
    int sp1 = cmd.indexOf(' ');
    int sp2 = cmd.indexOf(' ', sp1 + 1);
    int sp3 = cmd.indexOf(' ', sp2 + 1);
    if (sp3 > 0) {
      AlarmThresholds t{};
      t.current_warn_a  = cmd.substring(sp1 + 1, sp2).toFloat();
      t.current_alarm_a = cmd.substring(sp2 + 1, sp3).toFloat();
      t.temp_alarm_c    = cmd.substring(sp3 + 1).toFloat();
      t.current_drift_a = DEFAULT_CURRENT_DRIFT_A;
      if (g_alarm.setThresholds(t)) {
        Serial.println("OK: seuils mis a jour");
      } else {
        Serial.println("ERREUR: seuils invalides");
      }
    }
  } else if (cmd == "GPIO") {
    Serial.printf("USB=%d BT=%d HT=%d\n",
                  digitalRead(PWR_DETECT_USB_PIN),
                  digitalRead(PWR_DETECT_BT_PIN),
                  digitalRead(PWR_DETECT_HT_PIN));
  } else if (cmd == "STORAGE") {
    printStorageStats();
  } else if (cmd == "VERSION") {
    Serial.println("Ventec AGV Monitor v1.2");
  } else if (cmd == "BUZZER") {
    g_indicators.playTone(BUZZER_PWM_FREQ, 500);
    Serial.printf("OK: bip %u Hz, 500 ms (GPIO %d)\n", BUZZER_PWM_FREQ, BUZZER_PIN);
  } else if (cmd == "BUZZER WARN") {
    g_indicators.playPattern(AlarmLevel::WARNING);
    Serial.println("OK: motif warning (2 bips 1500 Hz)");
  } else if (cmd == "BUZZER ALARM" || cmd == "BUZZER CRIT") {
    g_indicators.playPattern(AlarmLevel::CRITICAL);
    Serial.println("OK: motif critique (3 bips 2500 Hz)");
  } else if (cmd.startsWith("BUZZER ")) {
    int sp = cmd.indexOf(' ', 7);
    if (sp > 0) {
      uint16_t freq = cmd.substring(7, sp).toInt();
      uint16_t ms   = cmd.substring(sp + 1).toInt();
      if (freq < 100) freq = BUZZER_PWM_FREQ;
      if (ms < 50) ms = 200;
      g_indicators.playTone(freq, ms);
      Serial.printf("OK: bip %u Hz, %u ms\n", freq, ms);
    } else {
      Serial.println("Usage: BUZZER <freq_Hz> <duree_ms>");
    }
  } else {
    Serial.printf("Commande inconnue: [%s] (len=%u). Tapez HELP ou VERSION.\n",
                  cmd.c_str(), cmd.length());
  }
}
