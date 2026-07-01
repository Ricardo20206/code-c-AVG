#pragma once

// =============================================================================
// Ventec AGV Monitoring Board - Configuration matérielle
// À adapter selon le schéma PCB final de l'équipe électronique.
// =============================================================================

// --- Bus I2C principal (Grove 1/2, MikroBus, capteurs embarqués) ---
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_FREQ_HZ         400000

// --- Mesure de courant (shunt + INA219/INA226 sur bus I2C) ---
// Shunt 2 mΩ AEC-Q200, gain interne INA219 → plage 0-32A
#define INA219_I2C_ADDR     0x40
#define SHUNT_RESISTOR_OHM  0.002f
#define MAX_CURRENT_A       30.0f
#define CURRENT_RESOLUTION_A 0.1f

// --- Sondes NTC PCB (diviseur résistif + filtrage condensateur) ---
#define NTC_PCB1_ADC_PIN    26
#define NTC_PCB2_ADC_PIN    25
#define NTC_SERIES_RES_OHM  10000.0f
#define NTC_BETA            3950.0f
#define NTC_NOMINAL_OHM     10000.0f
#define NTC_NOMINAL_TEMP_C  25.0f

// --- Température ambiante numérique (I2C) ---
#define AMBIENT_TEMP_I2C_ADDR 0x48   // TMP117

// --- Détection source d'alimentation (priorité matérielle USB>BT>HT) ---
#define PWR_DETECT_USB_PIN  32
#define PWR_DETECT_BT_PIN   33
#define PWR_DETECT_HT_PIN   25

// --- Indicateurs locaux ---
#define LED_GREEN_PIN       14
#define LED_RED_PIN         15
#define BUZZER_PIN          13
#define BUZZER_PWM_CHANNEL  0
#define BUZZER_PWM_FREQ     2000

// --- Boutons maintenance (FSMSM accessibles extérieur carter) ---
#define BTN_BOOT_PIN        0
#define BTN_RESET_PIN       16

// --- Connecteurs d'extension ---
#define GROVE1_I2C_ADDR_VIB 0x18     // LIS3DH (vibration)
#define GROVE2_I2C_ADDR_HUM 0x44     // SHT31 (humidité)
// MikroBus LoRa : interface SPI (SX1276), pas I2C

// --- RTC optionnel (DS3231) pour horodatage absolu ---
#define RTC_I2C_ADDR        0x68

// --- Seuils d'alarme par défaut (modifiables via BLE - EXF-23) ---
#define DEFAULT_CURRENT_WARN_A    15.0f
#define DEFAULT_CURRENT_ALARM_A   20.0f
#define DEFAULT_TEMP_ALARM_C      75.0f
#define DEFAULT_CURRENT_DRIFT_A   0.5f

// Durée minimale pour alarme courant continu (évite les pics transitoires)
#define CURRENT_ALARM_HOLD_MS     3000

// --- Périodes d'acquisition ---
#define MEASURE_INTERVAL_MS       500
#define BLE_NOTIFY_INTERVAL_MS    1000
#define LOG_INTERVAL_MS           5000

// --- Stockage ---
#define LOG_MAX_MEASUREMENTS      2000
#define LOG_MAX_ALARMS            200
#define LOG_MAX_SYSTEM_EVENTS     100
#define LOG_FILE_MEASUREMENTS     "/meas.bin"
#define LOG_FILE_ALARMS           "/alarms.bin"
#define LOG_FILE_SYSTEM           "/system.bin"
#define CONFIG_NAMESPACE          "ventec_cfg"

// --- BLE ---
#define BLE_DEVICE_NAME           "Ventec-AGV-Monitor"
