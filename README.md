# Ventec AGV Monitor — Firmware ESP32

Carte de monitoring embarquée pour les **AGV (Automated Guided Vehicles)** de **Ventec Systems**. Ce firmware C++ surveille en continu la consommation électrique de la batterie 36 V, les températures critiques du PCB et de l'ambiance, enregistre les événements horodatés et expose les données aux techniciens via **BLE** et **USB-C**, sans jamais couper l'alimentation haute tension.

---

## Table des matières

1. [Contexte et objectifs](#contexte-et-objectifs)
2. [Fonctionnalités](#fonctionnalités)
3. [Matériel requis](#matériel-requis)
4. [Structure du projet](#structure-du-projet)
5. [Architecture firmware](#architecture-firmware)
6. [Rôle détaillé des fichiers du code](#rôle-détaillé-des-fichiers-du-code)
7. [Mapping des exigences CDC](#mapping-des-exigences-cdc)
8. [Installation et compilation](#installation-et-compilation)
9. [Flash et moniteur série](#flash-et-moniteur-série)
10. [Commandes USB (CLI)](#commandes-usb-cli)
11. [API BLE GATT](#api-ble-gatt)
12. [Alarmes et seuils](#alarmes-et-seuils)
13. [Enregistrement des données](#enregistrement-des-données)
14. [Calibration courant (laboratoire)](#calibration-courant-laboratoire)
15. [Mode laboratoire](#mode-laboratoire)
16. [Configuration matérielle](#configuration-matérielle)
17. [Bus I2C](#bus-i2c)
18. [Documentation complémentaire](#documentation-complémentaire)
19. [Dépannage](#dépannage)
20. [Licence et auteurs](#licence-et-auteurs)

---

## Contexte et objectifs

Les AGV Ventec Systems sont alimentés en permanence par une batterie lithium **36 V**. Cette source ne peut pas être coupée en exploitation : cela désactiverait les capteurs de sécurité et la navigation.

Ce projet répond aux problèmes suivants :

- **Pics de consommation** non expliqués réduisant l'autonomie
- **Surchauffes localisées** sur les cartes électroniques embarquées
- **Absence de traçabilité** lors des pannes (aucun historique avant incident)

La carte de monitoring fonctionne en permanence sur le 36 V et permet au technicien de :

- Consulter l'état en temps réel via **BLE** (tablette/smartphone)
- Extraire les logs complets et mettre à jour le firmware via **USB-C**
- Recevoir des **alertes locales** (LED + buzzer) lors d'une intervention physique

---

## Fonctionnalités


| Domaine          | Description                                                         |
| ---------------- | ------------------------------------------------------------------- |
| **Courant**      | Mesure continue 0–30 A via shunt ROHM PSR400ITQFF0L50 (0,5 mΩ) + INA237AIDGST |
| **Température**  | 2 sondes NTC sur PCB (ADC) + capteur ambiant TMP126DCKR (SPI)           |
| **Alimentation** | Détection de la source active (USB > BT > HT), log des changements  |
| **Logging**      | Historique horodaté sur LittleFS avec rotation automatique          |
| **BLE**          | API GATT temps réel + modification des seuils sans reflash          |
| **USB**          | Export CSV, diagnostic, calibration, commandes maintenance          |
| **Alertes**      | LED verte permanente ; LED rouge + buzzer PWM en alarme           |
| **PSRAM**        | Tampon alarmes/mesures (IS66WVS1M8BLL 1 Mo, obligatoire en prod)  |
| **Extensions**   | Grove 1 (vibration LIS3DH), Grove 2 (humidité SHT31), MikroBus LoRa |
| **Calibration**  | Offset/gain courant stockés en NVS, procédure laboratoire intégrée  |


---

## Matériel requis

### Carte cible (Ventec AGV Monitor)


| Composant       | Référence                              | Rôle                            |
| --------------- | -------------------------------------- | ------------------------------- |
| MCU             | **ESP32-PICO-D4** (flash 4 Mo intégrée)| Traitement, BLE, Wi-Fi          |
| PSRAM           | **IS66WVS1M8BLL-104NLI** (1 Mo, quad)  | Tampon alarmes + anneau mesures |
| Capteur courant | INA237AIDGST @ 0x40                    | Lecture shunt haute courant     |
| Shunt           | **PSR400ITQFF0L50** (0,5 mΩ ±1 %, 4 W, AEC-Q200) | Mesure 0–30 A sur batterie 36 V |
| NTC ×2          | **NB12K00103JBB** (10 kΩ @ 25°C, B25/85 = 3630 K) | Température PCB (GPIO 26, 25) |
| Capteur ambiant | TMP126DCKR (SPI)                       | Température intérieur châssis   |
| RTC (optionnel) | DS3231 @ 0x68                          | Horodatage absolu               |
| LEDs            | Verte GPIO **14**, Rouge GPIO **15**   | Signalisation locale            |
| Buzzer          | GPIO 13 (PWM)                          | Alarme audible                  |
| USB-C           | —                                      | Flash firmware + export logs    |

> Carte de développement sans PSRAM : utiliser l'environnement `ventec_monitor_no_psram` (ESP32 DevKit).


### Outils de développement

- [PlatformIO](https://platformio.org/) (CLI ou extension VS Code / Cursor)
- Câble USB-C (données, pas charge seule)
- Python 3.x (pour `pip install platformio`)
- Smartphone avec **nRF Connect** (test BLE)
- Ampèremètre de référence (validation laboratoire EXF-10)

### Contraintes mécaniques PCB (CDC)

- Dimensions max : **70 × 50 mm**
- Composants sur face **TOP uniquement**
- USB-C, boutons BOOT/RESET, LEDs accessibles après intégration

---

## Structure du projet

```
code projet ESP32/
├── README.md                   # Ce fichier
├── platformio.ini              # Environnements build (prod / labo / devkit)
├── sdkconfig.defaults          # Référence GPIO PSRAM (CLK=6, CS=10)
├── boards/
│   └── ventec_agv_monitor.json # Board custom ESP32-PICO-D4
├── build.bat                   # Script de compilation Windows
├── monitor_sensors.ps1         # Affichage capteurs via USB
├── control_buzzer.ps1          # Test buzzer via USB
│
├── include/
│   ├── board_config.h          # GPIO, seuils, constantes matérielles
│   ├── psram_config.h          # Référence PSRAM + tableau câblage
│   └── types.h                 # Structures et énumérations
│
├── src/
│   ├── main.cpp                # Point d'entrée, boucle principale (v1.3)
│   ├── current_sensor.cpp      # Mesure courant (INA237AIDGST + calibration)
│   ├── temp_sensors.cpp        # NTC PCB + TMP126DCKR ambiant
│   ├── psram_pool.cpp          # Tampons PSRAM / RAM interne (mode labo)
│   ├── power_monitor.cpp       # Détection source d'alimentation
│   ├── alarm_manager.cpp       # Seuils, alarmes, dérive consommation
│   ├── data_logger.cpp         # LittleFS, ring buffer, export CSV
│   ├── ble_service.cpp         # API BLE GATT (NimBLE)
│   ├── indicators.cpp          # LEDs + buzzer PWM
│   ├── i2c_manager.cpp         # Capteurs Grove / détection bus
│   ├── calibration.cpp         # Calibration courant (NVS)
│   ├── lab_test.cpp            # Mode simulation laboratoire
│   ├── serial_cli.cpp          # Interface commandes USB
│   └── time_service.cpp        # Horodatage (RTC ou millis)
│
├── ventec-ble-api/             # SDK client BLE (PROTOCOL.md, app Android)
│
└── docs/
    ├── psram_wiring.md         # Câblage PSRAM PICO-D4 ↔ IS66WVS1M8BLL
    ├── i2c_address_map.md      # Tableau adresses I2C (EXF-31)
    ├── jalon1_architecture.md  # Stockage, alimentation, I2C
    ├── jalon2_ble_shunt.md     # Shunt, BLE GATT, boot/reset
    ├── jalon3_thermique.md     # Placement NTC, isolation thermique
    └── guide_validation_labo.md # Checklist validation complète
```

> Pour une description détaillée du rôle et du fonctionnement de **chaque fichier**, voir la section [Rôle détaillé des fichiers du code](#rôle-détaillé-des-fichiers-du-code).

---

## Architecture firmware

### Boucle principale (`main.cpp`)

```
┌─────────────────────────────────────────────────────────┐
│                      setup()                            │
│  Init: I2C, capteurs, logger, alarmes, BLE, LEDs       │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                      loop()                             │
│                                                         │
│  ┌──────────────┐  toutes les 500 ms                    │
│  │ Acquisition  │  courant, températures, humidité     │
│  └──────────────┘  → alarmes → mise à jour BLE         │
│                                                         │
│  ┌──────────────┐  toutes les 5 s                       │
│  │  Logging     │  enregistrement LittleFS              │
│  └──────────────┘                                       │
│                                                         │
│  ┌──────────────┐  toutes les 1 s                        │
│  │ BLE notify   │  push données connecté                 │
│  └──────────────┘                                       │
│                                                         │
│  ┌──────────────┐  toutes les 500 ms                     │
│  │ Indicateurs  │  LED verte / rouge + buzzer            │
│  └──────────────┘                                       │
│                                                         │
│  ┌──────────────┐  en continu                           │
│  │ CLI série    │  commandes technicien (USB)            │
│  │ Alimentation │  détection changement source          │
│  └──────────────┘                                       │
└─────────────────────────────────────────────────────────┘
```

### Modules et responsabilités


| Module          | Fichiers           | Rôle                                             |
| --------------- | ------------------ | ------------------------------------------------ |
| `CurrentSensor` | `current_sensor.*` | Lecture INA237AIDGST, lissage, application calibration |
| `TempSensors`   | `temp_sensors.*`   | Conversion NTC (Steinhart), TMP126DCKR SPI           |
| `PowerMonitor`  | `power_monitor.*`  | Lecture GPIO détection USB/BT/HT                 |
| `AlarmManager`  | `alarm_manager.*`  | Seuils, hystérésis 3 s, dérive 0,5 A, NVS        |
| `DataLogger`    | `data_logger.*`    | Ring buffer LittleFS, export CSV                 |
| `BleService`    | `ble_service.*`    | Serveur GATT NimBLE, notifications               |
| `Indicators`    | `indicators.*`     | LED verte permanente ; rouge + buzzer en alarme |
| `PsramPool`     | `psram_pool.*`     | Tampons alarmes/mesures (PSRAM ou RAM interne)  |
| `I2cManager`    | `i2c_manager.*`    | Grove vibration/humidité, scan bus               |
| `Calibration`   | `calibration.*`    | Gain/offset courant persisté en NVS              |
| `LabTest`       | `lab_test.*`       | Simulation sans AGV (validation EXF-10)          |
| `SerialCli`     | `serial_cli.*`     | Parser commandes USB maintenance                 |
| `TimeService`   | `time_service.*`   | Horodatage RTC DS3231 ou `millis()`              |


### Consommation mémoire (build release)


| Ressource | Utilisation      | Disponible |
| --------- | ---------------- | ---------- |
| RAM       | 36,9 Ko (11,3 %) | 320 Ko     |
| Flash     | 724 Ko (55,3 %)  | 1,31 Mo    |


---

## Rôle détaillé des fichiers du code

Cette section décrit le **rôle**, le **fonctionnement** et les **interactions** de chaque fichier du projet. L'objectif est de comprendre comment le firmware est structuré, même sans ouvrir le code source.

### Vue d'ensemble : flux de données

Le firmware peut être vu comme une chaîne de traitement :

```
Capteurs (courant, température, I2C)
        ↓
   main.cpp          ← orchestre tout
        ↓
 alarm_manager        → décide s'il y a un problème
        ↓
 data_logger          → enregistre l'historique
 ble_service          → envoie au technicien (BLE)
 indicators           → alerte locale (LED + buzzer)
 serial_cli           → interface technicien (USB)
```

Chaque fichier `.cpp` est un **module** avec une responsabilité précise. Les fichiers `.h` décrivent ce que le module expose aux autres modules.

### Schéma des dépendances

```
board_config.h ─────────────────────────────────────────┐
types.h ────────────────────────────────────────────────┤
                                                        ↓
              ┌─────────────────────────────────── main.cpp
              │                                        ↑
calibration.cpp ← current_sensor.cpp ──────────────────┤
temp_sensors.cpp ──────────────────────────────────────┤
power_monitor.cpp ─────────────────────────────────────┤
i2c_manager.cpp ───────────────────────────────────────┤
alarm_manager.cpp ──→ onAlarmTriggered() dans main.cpp ┤
psram_pool.cpp ────────────────────────────────────────┤
data_logger.cpp ←──────────────────────────────────────┤
ble_service.cpp ───────────────────────────────────────┤
indicators.cpp ────────────────────────────────────────┤
lab_test.cpp ──────────────────────────────────────────┤
serial_cli.cpp ────────────────────────────────────────┤
time_service.cpp ──────────────────────────────────────┘
```

**Règle générale :**

- Les modules **bas niveau** (capteurs, calibration) ne connaissent pas les modules haut niveau
- `main.cpp` est le seul fichier qui assemble tous les modules
- `board_config.h` et `types.h` sont inclus partout mais ne dépendent de rien

---

### Fichiers de configuration du projet

#### `platformio.ini`

Fichier de **build** : trois environnements selon la carte utilisée.


| Environnement              | Carte              | PSRAM   | Usage                                      |
| -------------------------- | ------------------ | ------- | ------------------------------------------ |
| `ventec_monitor` (défaut)  | `ventec_agv_monitor` | Obligatoire | Production Ventec (PICO-D4 + IS66WVS1M8BLL) |
| `ventec_monitor_lab`       | `ventec_agv_monitor` | Optionnelle | Tests sans PSRAM OK (LED, INA237, BLE…)    |
| `ventec_monitor_no_psram`  | `esp32dev`         | Désactivée | DevKit WROOM sans PSRAM                    |


| Paramètre                | Rôle                                                                |
| ------------------------ | ------------------------------------------------------------------- |
| `platform = espressif32` | SDK Espressif pour ESP32                                            |
| `board = ventec_agv_monitor` | ESP32-PICO-D4, flash DIO, `dio_qspi`                            |
| `board_build.psram = enabled` | Active la PSRAM externe                                        |
| `framework = arduino`      | Framework Arduino                                                   |
| `monitor_speed = 115200`   | Vitesse du port série USB                                           |
| `lib_deps`               | NimBLE, INA237, TMP126, LIS3DH, SHT31, RTClib                       |


Sans ce fichier, PlatformIO ne sait pas quoi compiler ni quelles bibliothèques utiliser.

#### `build.bat`

Script Windows **raccourci** : installe PlatformIO si absent, puis lance la compilation. Pratique pour compiler en double-cliquant sans taper de commandes.

---

### Fichiers d'en-tête partagés (`include/`)

#### `include/board_config.h`

**Plan de câblage logiciel** de la carte. C'est le fichier le plus important à adapter quand le schéma PCB est connu.

Il centralise toutes les constantes matérielles :

- **GPIO** : quelle broche ESP32 est branchée à quoi (LED, buzzer, NTC, détection alim)
- **Adresses I2C** : 0x40 pour INA237AIDGST, etc. TMP126DCKR est sur SPI (pas I2C).
- **Seuils par défaut** : 15 A warning, 20 A alarme, 75 °C
- **Périodes** : mesure toutes les 500 ms, log toutes les 5 s
- **Limites stockage** : 2000 mesures max, 200 alarmes max

**Pourquoi un fichier séparé ?** Pour ne pas disperser les numéros de broches dans tout le code : un seul endroit à modifier lors d'un changement hardware.

#### `include/types.h`

**Dictionnaire de données** du projet. Il définit les structures et énumérations utilisées par tous les modules :


| Type                | Rôle                                                        |
| ------------------- | ----------------------------------------------------------- |
| `PowerSource`       | Source d'alimentation : `USB_C`, `BT_SVC`, `HT_36V`         |
| `AlarmLevel`        | Niveau d'alarme : `NONE`, `WARNING`, `CRITICAL`             |
| `AlarmType`         | Type d'alarme : courant, température, dérive                |
| `MeasurementRecord` | Une mesure horodatée complète (courant, températures, etc.) |
| `AlarmRecord`       | Une alarme avec sa valeur de déclenchement et son seuil     |
| `SystemEventRecord` | Un événement système (boot, changement alim, connexion)     |
| `LiveData`          | Snapshot de l'état instantané de la carte                   |


C'est le **contrat de données** entre tous les modules : tout le monde parle le même langage.

---

### Le chef d'orchestre

#### `src/main.cpp` (+ pas de `.h` dédié)

**Point d'entrée** du programme. Seul fichier avec `setup()` et `loop()` (obligatoire en Arduino).

`**setup()`** — exécuté une fois au démarrage :

1. Allume la **LED verte** (GPIO 14) immédiatement
2. Ouvre le port série USB (115200 baud)
3. Initialise PSRAM (`g_psram`) — arrêt si obligatoire et échec (`ventec_monitor`)
4. Initialise les modules : `g_time`, `g_i2c`, `g_power`, `g_current`, `g_temp`, `g_logger`, `g_alarm`, `g_indicators`, `g_ble`
5. Enregistre l'événement de boot dans les logs
6. Vérifie si l'INA237AIDGST répond sur le bus I2C

`**loop()**` — exécuté en boucle infinie, cadencé par des timers :


| Timer       | Période | Action                                                  |
| ----------- | ------- | ------------------------------------------------------- |
| Mesure      | 500 ms  | Lit capteurs, évalue alarmes, met à jour BLE            |
| Log         | 5 s     | Sauvegarde une mesure sur LittleFS                      |
| BLE         | 1 s     | Envoie notifications si technicien connecté             |
| Indicateurs | 500 ms  | Met à jour LEDs et buzzer                               |
| Continu     | —       | Détecte changement d'alimentation, traite commandes USB |


**Fonctions internes importantes :**


| Fonction                | Rôle                                                              |
| ----------------------- | ----------------------------------------------------------------- |
| `acquireMeasurements()` | Lit tous les capteurs et remplit `g_live`                         |
| `persistMeasurement()`  | Transforme `g_live` en `MeasurementRecord` et l'enregistre        |
| `onAlarmTriggered()`    | Callback appelé par `alarm_manager` quand une alarme se déclenche |
| `handlePowerChange()`   | Détecte si la source d'alimentation a changé et log l'événement   |
| `logBootEvent()`        | Enregistre le démarrage dans `/system.bin`                        |


La variable globale `g_live` est le **tableau de bord instantané** : toutes les valeurs courantes en un seul endroit, partagées entre BLE, CLI et logging.

---

### Modules capteurs

#### `src/current_sensor.h` + `src/current_sensor.cpp`

**Rôle :** mesurer le courant tiré sur la batterie 36 V (EXF-07 à EXF-09).

**Fonctionnement :**

1. Communique avec le chip **INA237AIDGST** sur le bus I2C (adresse 0x40)
2. L'INA237AIDGST mesure la tension aux bornes du **shunt PSR400ITQFF0L50 (0,5 mΩ)** placé en série avec la batterie
3. Configure l'INA237 via `setShunt(0.0005 Ω, 30 A)` pour la conversion tension → courant
4. Passe la valeur brute dans `g_calib.apply()` pour la calibration terrain
5. Applique un **lissage exponentiel** (70 % ancienne valeur + 30 % nouvelle) pour filtrer le bruit

**Méthodes exposées :**


| Méthode          | Description                                                     |
| ---------------- | --------------------------------------------------------------- |
| `begin()`        | Initialise l'INA237AIDGST                                             |
| `readCurrentA()`       | Retourne le courant calibré et lissé en ampères                 |
| `readRawAmps()`        | Retourne la valeur brute avant calibration (pour `CAL` en labo) |
| `readShuntVoltageV()`  | Tension réelle aux bornes du shunt (V)                          |
| `readShuntVoltage_mV()`| Même mesure en millivolts                                       |
| `isHealthy()`          | `true` si l'INA237AIDGST répond sur I2C                        |


#### `src/temp_sensors.h` + `src/temp_sensors.cpp`

**Rôle :** mesurer 3 températures (EXF-11 à EXF-13).

**Deux technologies différentes :**

**NTC PCB 1 et 2** (GPIO **26** et **25**, entrées ADC) :

```
3,3V ──[10kΩ]──┬── NTC ── GND
               ├── 100 nF ── GND
               └── GPIO ADC
```

1. Lit la valeur ADC (0–4095 sur 12 bits)
2. Calcule la tension au point milieu du diviseur
3. Déduit la résistance de la NTC
4. Applique l'équation de **Steinhart-Hart** (coefficient β = 3630, B25/85 NB12K00103JBB) pour obtenir °C

**TMP126DCKR ambiant** (SPI : CS=5, MOSI=23, MISO=19, SCLK=18) :

- Capteur numérique haute précision
- Mesure la température à l'intérieur du châssis AGV
- Retourne -999 si le capteur est absent

#### `src/power_monitor.h` + `src/power_monitor.cpp`

**Rôle :** savoir quelle source alimente la carte (EXF-17).

Le circuit matériel (ORing) assure la priorité **USB > BT > HT**. Le firmware lit simplement 3 GPIO :


| GPIO | HIGH signifie              |
| ---- | -------------------------- |
| 35   | USB-C actif                |
| 27   | Batterie de service active |
| 4    | Batterie 36 V active       |


`hasSourceChanged()` compare l'état actuel au précédent : si différent, `main.cpp` enregistre un événement `POWER_CHANGE` dans les logs.

---

### Module alarmes

#### `src/alarm_manager.h` + `src/alarm_manager.cpp`

**Rôle :** le **cerveau décisionnel** — décide s'il y a un problème (EXF-24, EXF-25).

**Logique d'évaluation** (appelée toutes les 500 ms depuis `main.cpp`) :

```
Courant ≥ 20 A pendant 3 s ?  → CRITIQUE
Courant ≥ 15 A pendant 3 s ?  → WARNING
T° PCB ≥ 75 °C ?              → CRITIQUE
```

Le délai de **3 secondes** (`CURRENT_ALARM_HOLD_MS`) évite les fausses alarmes sur les pics transitoires (accélération, rampe).

**Dérive de consommation** (EXF-09) :

- Toutes les 60 s, met à jour une baseline (moyenne glissante du courant)
- Si écart ≥ 0,5 A → alarme `CURRENT_DRIFT` enregistrée

**Persistance des seuils :**

- Chargés depuis la NVS au démarrage (`loadThresholds()`)
- Modifiables via BLE ou commande USB `THRESH`
- Sauvegardés en NVS (`saveThresholds()`) — survivent au redémarrage

Quand une alarme **change de niveau**, le module appelle `onAlarmTriggered()` dans `main.cpp` qui l'enregistre en LittleFS et en PSRAM.

---

### Module stockage

#### `src/data_logger.h` + `src/data_logger.cpp`

**Rôle :** la **mémoire à long terme** de la carte (EXF-15 à EXF-18).

Utilise le système de fichiers **LittleFS** (flash interne ESP32) avec 3 fichiers :


| Fichier       | Contenu                           | Max  |
| ------------- | --------------------------------- | ---- |
| `/meas.bin`   | Mesures périodiques               | 2000 |
| `/alarms.bin` | Événements d'alarme               | 200  |
| `/system.bin` | Boot, changement alim, connexions | 100  |


**Algorithme ring buffer** (`appendRingFile`) :

```
[M1][M2][M3]...[M1999]  ← plein
 Nouvelle mesure M2000 arrive
 → on supprime M1, on décale tout, on ajoute M2000 à la fin
[M2][M3]...[M1999][M2000]
```

**Export CSV** (`exportAllCsv`) : lit tous les fichiers et produit un texte CSV envoyé sur USB quand le technicien tape `EXPORT`.

---

### Module communication BLE

#### `src/ble_service.h` + `src/ble_service.cpp`

**Rôle :** interface **sans fil** pour le technicien avec sa tablette (EXF-19, EXF-23).

Utilise la bibliothèque **NimBLE** (stack BLE allégé pour ESP32).

**Structure GATT créée au démarrage :**

```
Service Ventec (UUID 6e400001-...)
├── Caractéristique Courant      (...0002)  READ + NOTIFY
├── Caractéristique Températures (...0003)  READ + NOTIFY
├── Caractéristique Alarme       (...0004)  READ + NOTIFY
├── Caractéristique Seuils       (...0005)  READ + WRITE  ← modifiable
├── Caractéristique Logs         (...0006)  READ
└── Caractéristique Statut       (...0007)  READ + NOTIFY
```

**3 classes callback internes :**


| Classe               | Rôle                                                            |
| -------------------- | --------------------------------------------------------------- |
| `ServerCallbacks`    | Détecte connexion/déconnexion technicien, relance l'advertising |
| `ThresholdCallbacks` | Reçoit les nouveaux seuils écrits par la tablette               |
| `LogsCallbacks`      | Renvoie le CSV quand le technicien lit la caractéristique logs  |


`notifyIfConnected()` est appelée chaque seconde depuis `main.cpp` : pousse les données vers la tablette si quelqu'un est connecté.

---

### Module indicateurs locaux

#### `src/indicators.h` + `src/indicators.cpp`

**Rôle :** signalisation **physique** sur la carte pour le technicien présent sur site (EXF-24 à EXF-27).


| État                      | LED verte (GPIO 14) | LED rouge (GPIO 15) | Buzzer                          |
| ------------------------- | ------------------- | ------------------- | ------------------------------- |
| Normal                    | ON permanente       | OFF                 | OFF                             |
| Warning (>15 A)           | ON permanente       | Clignote            | Clignote + double bip 1500 Hz   |
| Critique (>20 A ou >75°C) | ON permanente       | ON fixe             | ON continu + triple bip 2500 Hz |


Le buzzer utilise le **PWM** (LED Control du ESP32) : `ledcWriteTone()` permet de choisir la fréquence, ce qui crée des motifs sonores distincts (EXF-27).

---

### Module bus I2C et extensions

#### `src/i2c_manager.h` + `src/i2c_manager.cpp`

**Rôle :** gérer le bus I2C partagé et les **capteurs optionnels** (EXF-28 à EXF-30).

Au démarrage :

1. Initialise le bus I2C (SDA=21, SCL=22, 400 kHz)
2. Tente de détecter le **LIS3DH** (vibration, Grove 1, adresse 0x18)
3. Tente de détecter le **SHT31** (humidité, Grove 2, adresse 0x44)
4. Placeholder pour le module **LoRa** (SPI, pas encore implémenté)

**Design tolérant aux pannes** : si un capteur Grove est absent, le firmware continue normalement. `vibrationPresent()` et `humidityPresent()` indiquent ce qui est branché.

---

### Module calibration

#### `src/calibration.h` + `src/calibration.cpp`

**Rôle :** corriger l'erreur de mesure du courant en laboratoire (EXF-10).

Formule appliquée :

```
courant_réel = (courant_brut - offset) × gain
```

- `offset` et `gain` sont stockés en **NVS** (mémoire flash non volatile ESP32)
- `calibrateWithReference(raw, 10.0)` : le technicien applique 10 A réels, le firmware calcule le gain
- `reset()` : remet offset=0, gain=1

Appelé automatiquement par `current_sensor.cpp` à chaque lecture via `g_calib.apply()`.

---

### Module laboratoire

#### `src/lab_test.h` + `src/lab_test.cpp`

**Rôle :** tester le firmware **sans AGV connecté** (EXF-10).

Quand `LABON` est tapé :

- `main.cpp` n'appelle plus les vrais capteurs courant/température
- `applyTo(g_live)` injecte des valeurs simulées à la place
- Les alarmes, LEDs, buzzer et logging fonctionnent normalement

Exemple : `SIM 22` simule 22 A → alarme critique après 3 secondes, sans aucun courant réel.

---

### Module interface USB

#### `src/serial_cli.h` + `src/serial_cli.cpp`

**Rôle :** le **terminal de commande** pour le technicien via USB-C (EXF-21).

Lit les lignes tapées dans le moniteur série et les route vers la bonne action :


| Commande        | Action interne                             |
| --------------- | ------------------------------------------ |
| `STATUS`        | Affiche `g_live`                           |
| `EXPORT`        | Appelle `g_logger.exportAllCsv()`          |
| `CAL 10`        | Appelle `g_calib.calibrateWithReference()` |
| `I2CSCAN`       | Scanne le bus Wire de 0x01 à 0x7F          |
| `THRESH`        | Lit ou modifie `g_alarm.setThresholds()`   |
| `LABON` / `SIM` | Active le mode simulation via `g_lab`      |


C'est l'interface **humaine** du firmware : tout ce qu'un technicien peut faire sans recompiler.

---

### Module horodatage

#### `src/time_service.h` + `src/time_service.cpp`

**Rôle :** fournir un **timestamp** pour tous les enregistrements (EXF-15, EXF-16).

Deux modes :

1. **RTC DS3231 présent** (I2C 0x68) → horodatage absolu (date/heure réelle)
2. **RTC absent** → utilise `millis()` (temps relatif depuis le boot)

Tous les modules appellent `g_time.nowMs()` pour horodater leurs enregistrements. Un seul point de vérité pour le temps.

---

### Fichiers de documentation (`docs/`)

Ces fichiers ne sont **pas compilés** : ils documentent et justifient les choix techniques face au client Ventec.


| Fichier                    | Contenu                                             | Public               |
| -------------------------- | --------------------------------------------------- | -------------------- |
| `i2c_address_map.md`       | Tableau des adresses I2C, vérification des conflits | Équipe électronique  |
| `jalon1_architecture.md`   | Stockage, rotation, alimentation                    | Présentation Jalon 1 |
| `jalon2_ble_shunt.md`      | Câblage shunt IN+/IN-, API GATT, boot/reset         | Présentation Jalon 2 |
| `jalon3_thermique.md`      | Placement NTC, isolation thermique                  | Présentation Jalon 3 |
| `guide_validation_labo.md` | Checklist pas-à-pas de validation                   | Technicien / testeur |


---

### Résumé : un fichier, une phrase


| Fichier              | En une phrase                                              |
| -------------------- | ---------------------------------------------------------- |
| `platformio.ini`     | Configure la compilation et les bibliothèques              |
| `build.bat`          | Raccourci Windows pour compiler                            |
| `board_config.h`     | Toutes les constantes matérielles (GPIO, seuils, adresses) |
| `types.h`            | Toutes les structures de données partagées                 |
| `main.cpp`           | Orchestre tout : init, boucle, timers, coordination        |
| `current_sensor.cpp` | Lit le courant batterie via INA237AIDGST + shunt                 |
| `temp_sensors.cpp`   | Lit 2 NTC (GPIO 26/25) + TMP126DCKR (SPI)                  |
| `psram_pool.cpp`     | Tampons alarmes/mesures PSRAM ou RAM interne               |
| `power_monitor.cpp`  | Détecte quelle source alimente la carte                    |
| `alarm_manager.cpp`  | Décide s'il y a une alarme, gère les seuils                |
| `data_logger.cpp`    | Enregistre l'historique sur flash (ring buffer)            |
| `ble_service.cpp`    | Expose les données au technicien via Bluetooth             |
| `indicators.cpp`     | Contrôle LED verte/rouge et buzzer                         |
| `i2c_manager.cpp`    | Gère le bus I2C et capteurs Grove optionnels               |
| `calibration.cpp`    | Corrige la mesure de courant (offset/gain en NVS)          |
| `lab_test.cpp`       | Simule des valeurs pour tester sans AGV                    |
| `serial_cli.cpp`     | Interface commandes USB pour le technicien                 |
| `time_service.cpp`   | Fournit l'horodatage pour tous les logs                    |


Chaque paire `.h` / `.cpp` suit la convention C++ classique : le `.h` déclare l'interface publique de la classe, le `.cpp` contient l'implémentation.

---

## Mapping des exigences CDC

### Exigences couvertes par le firmware


| ID        | Priorité    | Description                                      | Module                         |
| --------- | ----------- | ------------------------------------------------ | ------------------------------ |
| EXF-07    | OBLIGATOIRE | Mesure courant continue 36 V                     | `current_sensor`               |
| EXF-08    | OBLIGATOIRE | Shunt AEC-Q200 PSR400ITQFF0L50 (0,5 mΩ)          | `board_config.h`               |
| EXF-09    | OBLIGATOIRE | Plage 0–30 A, dérive ≥ 0,5 A                     | `alarm_manager`                |
| EXF-10    | OBLIGATOIRE | Validation labo (points test, simulation)        | `lab_test`, `calibration`      |
| EXF-11    | OBLIGATOIRE | 2 sondes NTC PCB                                 | `temp_sensors`                 |
| EXF-12    | OBLIGATOIRE | Diviseur + filtrage ADC NTC                      | `temp_sensors`                 |
| EXF-13    | OBLIGATOIRE | Capteur température ambiante numérique           | `temp_sensors` (TMP126DCKR)        |
| EXF-15    | OBLIGATOIRE | Historique horodaté courant + température        | `data_logger`                  |
| EXF-16    | OBLIGATOIRE | Alarmes avec timestamp et valeur                 | `data_logger`, `alarm_manager` |
| EXF-17    | OBLIGATOIRE | Logs système (boot, alim, connexions)            | `data_logger`, `main`          |
| EXF-18    | IMPORTANT   | Rotation des données (ring buffer)               | `data_logger`                  |
| EXF-19    | OBLIGATOIRE | API BLE temps réel                               | `ble_service`                  |
| EXF-21    | OBLIGATOIRE | Export logs + flash via USB-C                    | `serial_cli`                   |
| EXF-23    | IMPORTANT   | Modification seuils via BLE                      | `ble_service`                  |
| EXF-24    | IMPORTANT   | LED verte en fonctionnement normal               | `indicators`                   |
| EXF-25    | IMPORTANT   | LED rouge + buzzer sur alarme, log PSRAM         | `indicators`, `main`           |
| EXF-26    | IMPORTANT   | Buzzer piloté ESP32 (fréquence/durée)            | `indicators`                   |
| EXF-27    | SOUHAITABLE | Motifs sonores/lumineux distincts                | `indicators`                   |
| EXF-28–30 | IMPORTANT   | Capteurs Grove + MikroBus LoRa                   | `i2c_manager`                  |
| EXF-31    | OBLIGATOIRE | Table I2C documentée, pas de conflit             | `docs/i2c_address_map.md`      |


### Exigences principalement matérielles (hors firmware)


| ID              | Description                                             |
| --------------- | ------------------------------------------------------- |
| EXF-01 à EXF-06 | Alimentation 36V/USB/BT, priorité matérielle, isolation |
| EXF-20          | Antenne BLE, plan de masse, filtrage π                  |
| EXF-22          | Protection EMI/ESD lignes USB                           |
| EXF-32 à EXF-34 | Automatisation boot/reset (circuit DTR/RTS)             |
| EXF-35 à EXF-37 | Contraintes mécaniques PCB mono-face                    |


---

## Installation et compilation

### Prérequis

```powershell
pip install platformio
```

### Compilation

```powershell
cd "c:\Users\mberi\Desktop\code projet ESP32"
python -m platformio run                    # production (ventec_monitor)
python -m platformio run -e ventec_monitor_lab   # carte Ventec, PSRAM optionnelle
python -m platformio run -e ventec_monitor_no_psram  # DevKit sans PSRAM
```

Ou double-cliquer sur `build.bat` (Windows).

### Environnements PlatformIO


| Environnement             | Board                | PSRAM        | Quand l'utiliser                    |
| ------------------------- | -------------------- | ------------ | ----------------------------------- |
| `ventec_monitor`          | `ventec_agv_monitor` | Obligatoire  | Carte Ventec finale                 |
| `ventec_monitor_lab`      | `ventec_agv_monitor` | Optionnelle  | Debug PSRAM / tests capteurs sur PCB |
| `ventec_monitor_no_psram` | `esp32dev`           | Désactivée   | ESP32 DevKit WROOM                  |


| Paramètre        | Valeur production   |
| ---------------- | ------------------- |
| Plateforme       | `espressif32` 7.0.1 |
| MCU              | ESP32-PICO-D4       |
| Framework        | Arduino             |
| Vitesse moniteur | 115200 baud         |


### Bibliothèques externes


| Bibliothèque    | Version | Usage                |
| --------------- | ------- | -------------------- |
| NimBLE-Arduino  | ^1.4.2  | Stack BLE            |
| Adafruit INA237 and INA238 Library | latest | Mesure courant       |
| TMP126 (edovis)                    | 1.0.0  | Température ambiante |
| Adafruit LIS3DH | ^1.2.4  | Vibration (Grove 1)  |
| Adafruit SHT31  | ^2.2.2  | Humidité (Grove 2)   |
| RTClib          | ^2.1.4  | RTC DS3231 optionnel |


---

## Flash et moniteur série

### Flasher la carte

```powershell
# Production Ventec
python -m platformio run -e ventec_monitor -t upload --upload-port COM3

# Mode labo (PSRAM non bloquante)
python -m platformio run -e ventec_monitor_lab -t upload --upload-port COM3
```

> Fermer le moniteur série avant l'upload (`Ctrl+C`). Si le port est occupé ou introuvable, vérifier le câble USB et lancer `python -m platformio device list`.

Reflash propre après changement PSRAM :

```powershell
python -m platformio run -e ventec_monitor -t fullclean
python -m platformio run -e ventec_monitor -t upload --upload-port COM3
```

### Ouvrir le moniteur série

```powershell
python -m platformio device monitor
```

Au démarrage, la carte affiche :

```
Ventec AGV Monitor v1.3 - demarrage
Pret. Tapez HELP pour les commandes.
```

En production, si la PSRAM est OK :

```
PSRAM IS66WVS1M8BLL-104NLI : init (PICO-D4, CLK=GPIO6 CS=GPIO10)
  psramFound()     : oui
  ESP.getPsramSize : 1024 Ko
```

### Reprogrammation terrain (EXF-34)

La procédure est entièrement automatisée via USB-C :

1. Brancher le câble USB-C
2. Lancer `python -m platformio run -t upload`
3. L'outil pilote automatiquement les signaux DTR/RTS (boot/reset)
4. Durée typique : **30 à 60 secondes**

Les boutons BOOT (GPIO 0) et RESET (GPIO 16) restent accessibles en secours manuel.

---

## Commandes USB (CLI)

Toutes les commandes sont envoyées via le moniteur série (115200 baud). Insensible à la casse.

### Commandes générales


| Commande           | Description                                             |
| ------------------ | ------------------------------------------------------- |
| `HELP`             | Affiche la liste des commandes                          |
| `STATUS`           | État instantané (courant, tension shunt, températures, alarme, source) |
| `EXPORT` ou `LOGS` | Export CSV complet de l'historique                      |
| `CLEAR`            | Efface tous les logs                                    |
| `GPIO`             | État des broches de détection d'alimentation            |
| `STORAGE`          | Statistiques de stockage (volumes, compteurs)           |
| `PSRAM`            | État PSRAM / tampons alarmes                            |
| `SENSORS`          | Affichage détaillé de tous les capteurs                 |
| `MONITOR ON/OFF`   | Affichage capteurs toutes les 1 s                       |
| `VERSION`          | Version firmware (v1.3)                                 |


### Diagnostic et bus I2C


| Commande                       | Description                                      |
| ------------------------------ | ------------------------------------------------ |
| `I2CSCAN`                      | Scan du bus I2C (détecte tous les périphériques) |
| `RAW`                          | Courant brut INA237 + tension shunt (mV / µV) + I=V/R   |
| `THRESH`                       | Affiche les seuils d'alarme actuels              |
| `THRESH <warn> <alarm> <temp>` | Modifie les seuils (ex: `THRESH 15 20 75`)       |


### Calibration courant (EXF-10)


| Commande   | Description                                                 |
| ---------- | ----------------------------------------------------------- |
| `CAL <A>`  | Calibre avec un courant de référence connu (ex: `CAL 10.0`) |
| `CALRESET` | Réinitialise offset et gain à leurs valeurs par défaut      |


**Procédure recommandée :**

1. Appliquer une charge connue (ex. 10 A mesurés à l'ampèremètre)
2. `RAW` → vérifier courant brut et tension shunt (~5 mV à 10 A pour 0,5 mΩ)
3. `CAL 10.0` → appliquer la calibration
4. `STATUS` → vérifier que le courant affiché ≈ 10,0 A (±0,5 A) et que **I=V/R** est cohérent

**Tensions shunt attendues (PSR400ITQFF0L50, R = 0,5 mΩ) :**

| Courant | Tension shunt |
| ------- | ------------- |
| 1 A     | 0,5 mV        |
| 10 A    | 5 mV          |
| 20 A    | 10 mV         |
| 30 A    | 15 mV         |

### Tests capteurs (INA237, NTC PCB)

**INA237 (courant)** — mode réel (`LABOFF`) :

```text
I2CSCAN          # doit afficher 0x40
RAW              # courant brut + tension shunt (mV)
STATUS           # courant calibré + shunt + températures
CAL 10.0         # calibration avec 10 A de référence
```

**NTC PCB 1 / PCB 2** — mode réel (`LABOFF`) :

```text
STATUS           # T° PCB1 (GPIO 26), PCB2 (GPIO 25), ambiante
SENSORS          # détail capteurs
MONITOR ON       # affichage continu 1 Hz
```

Test alarme température sans chauffer la carte :

```text
LABON
SIMTEMP 80       # PCB1
SIMTEMP2 80      # PCB2
LABOFF
```

Script PC : `.\monitor_sensors.ps1 -Port COM3`

### Mode laboratoire


| Commande        | Description                                         |
| --------------- | --------------------------------------------------- |
| `LABON`         | Active le mode simulation (sans AGV)                |
| `LABOFF`        | Désactive le mode simulation (capteurs réels)       |
| `SIM <A>`       | Simule un courant (ex: `SIM 22` pour tester alarme) |
| `SIMTEMP <C>`   | Simule la température PCB1 (ex: `SIMTEMP 80`)       |
| `SIMTEMP2 <C>`  | Simule la température PCB2                          |
| `SIMAMB <C>`    | Simule la température ambiante                      |
| `SIMHUM <%>`    | Simule l'humidité                                   |
| `SIMVIB <mg>`   | Simule la vibration                                 |


---

## API BLE GATT

### Connexion

1. Ouvrir **nRF Connect** sur smartphone/tablette
2. Scanner les périphériques BLE
3. Se connecter à `**Ventec-AGV-Monitor`**

### Service principal


| Élément                | UUID                                   |
| ---------------------- | -------------------------------------- |
| Service Ventec Monitor | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |


### Caractéristiques


| Nom          | UUID (suffixe) | Type             | Accès        | Contenu                          |
| ------------ | -------------- | ---------------- | ------------ | -------------------------------- |
| Courant      | `...0002`      | float32          | READ, NOTIFY | Courant instantané (A)           |
| Températures | `...0003`      | float32 × 3      | READ, NOTIFY | PCB1, PCB2, ambiante (°C)        |
| Alarme       | `...0004`      | uint8            | READ, NOTIFY | 0=aucune, 1=warning, 2=critique  |
| Seuils       | `...0005`      | struct 16 octets | READ, WRITE  | warn_A, alarm_A, temp_C, drift_A |
| Logs         | `...0006`      | string CSV       | READ         | Export historique complet        |
| Statut       | `...0007`      | uint8 × 4        | READ, NOTIFY | source, vib, hum, lora           |
| Télémétrie   | `...0008`      | 10 octets        | READ, NOTIFY | humidité, vibration, flags       |
| Alarme event | `...0009`      | 14 octets        | READ, NOTIFY | événement alarme (notify immédiat) |
| Alarmes hist.| `...000a`      | binaire          | READ         | tampon PSRAM (jusqu'à 50)        |


### Écriture des seuils via BLE (EXF-23)

Écrire 16 octets (4 floats IEEE 754) sur la caractéristique `...0005` :

```
[float32] current_warn_a   (ex: 15.0)
[float32] current_alarm_a  (ex: 20.0)
[float32] temp_alarm_c     (ex: 75.0)
[float32] current_drift_a  (ex: 0.5)
```

Les seuils sont persistés en NVS et survivent au redémarrage.

### SDK client tablette

Le dossier [`ventec-ble-api/`](ventec-ble-api/README.md) contient :

- `PROTOCOL.md` — spécification octets / UUID
- `android/` — parsers Kotlin réutilisables
- `android-app/` — **app Android minimale** (écran technicien BLE)

---

## Alarmes et seuils

### Niveaux d'alarme


| Niveau       | Condition                                        | Signalisation                                      |
| ------------ | ------------------------------------------------ | -------------------------------------------------- |
| **Normal**   | Tout dans les limites                            | LED verte permanente ; rouge éteinte               |
| **Warning**  | Courant ≥ 15 A pendant 3 s                       | Verte permanente ; rouge clignotante + double bip  |
| **Critique** | Courant ≥ 20 A pendant 3 s **ou** T° PCB ≥ 75 °C | Verte permanente ; rouge fixe + buzzer continu     |


### Détection de dérive (EXF-09)

Toutes les 60 secondes, le firmware compare le courant instantané à une baseline glissante. Un écart ≥ **0,5 A** déclenche une alarme de type `CURRENT_DRIFT` enregistrée dans les logs.

### Seuils par défaut


| Paramètre       | Valeur | Modifiable      |
| --------------- | ------ | --------------- |
| Courant warning | 15,0 A | BLE / USB / NVS |
| Courant alarme  | 20,0 A | BLE / USB / NVS |
| Température PCB | 75 °C  | BLE / USB / NVS |
| Dérive courant  | 0,5 A  | NVS             |


### Anti-rebond

Les alarmes de courant nécessitent une durée minimale de **3 secondes** (`CURRENT_ALARM_HOLD_MS`) pour éviter les déclenchements sur les pics transitoires (accélération, rampe).

---

## Enregistrement des données

### Fichiers LittleFS


| Fichier       | Type d'enregistrement | Capacité max | Taille unitaire |
| ------------- | --------------------- | ------------ | --------------- |
| `/meas.bin`   | Mesures périodiques   | 2000         | 28 octets       |
| `/alarms.bin` | Événements d'alarme   | 200          | 20 octets       |
| `/system.bin` | Événements système    | 100          | 9 octets        |


**Volume total estimé : ~61 Ko**

### Politique de rotation (EXF-18)

Lorsque la capacité d'un fichier est atteinte, l'entrée la plus ancienne est écrasée (ring buffer). L'enregistrement continue indéfiniment sans intervention du technicien.

### Événements système enregistrés


| Type               | Déclencheur                         |
| ------------------ | ----------------------------------- |
| `BOOT`             | Démarrage de la carte               |
| `POWER_CHANGE`     | Changement de source d'alimentation |
| `TECH_USB_CONN`    | Connexion USB technicien            |
| `TECH_BLE_CONN`    | Connexion BLE technicien            |
| `THRESHOLD_CHANGE` | Modification des seuils             |
| `SENSOR_FAULT`     | INA237 ou PSRAM indisponible              |


### Tampon PSRAM (EXF-25)

Les 50 dernières alarmes et un anneau de 128 mesures sont conservés en **PSRAM** (IS66WVS1M8BLL, 1 Mo) pour un accès rapide via BLE (`...000a`), en complément du stockage persistant LittleFS.

Câblage PSRAM (bus SDIO partagé flash intégrée) : voir [`docs/psram_wiring.md`](docs/psram_wiring.md).

| Signal PSRAM | GPIO ESP32-PICO-D4 |
| ------------ | ------------------ |
| CLK          | 6 (SD_CLK)         |
| CE#          | 10 (SD_DATA_3)     |
| SIO0         | 8                  |
| SIO1         | 17                 |
| SIO2         | 7                  |
| SIO3         | 11                 |

En mode `ventec_monitor_lab`, si la PSRAM échoue, les tampons basculent en **RAM interne** et le firmware continue.

### Export CSV

```
EXPORT
```

Format de sortie :

```csv
type,timestamp_ms,v1,v2,v3,v4
MEAS,12345678,5.23,35.1,38.2,25.0
ALARM,12350000,1,2,22.50,20.00
SYS,12300000,1,0,0,0
```

---

## Calibration courant (laboratoire)

### Principe

Le firmware applique une correction linéaire au courant brut de l'INA237AIDGST :

```
courant_calibré = (courant_brut - offset) × gain
```

Les coefficients `offset` et `gain` sont stockés en **NVS** (namespace `ventec_cfg`) et persistent après redémarrage.

### Matériel de test (EXF-10)

- Alimentation 36 V simulée
- Résistances de charge calibrées :
  - 3,6 Ω → ~10 A
  - 1,8 Ω → ~20 A
- Ampèremètre de référence sur les points de test `TP_SHUNT+` / `TP_SHUNT-`
- Multimètre pour vérifier la tension aux bornes du shunt (ex. ~5 mV à 10 A avec PSR400ITQFF0L50)

### Points de test PCB


| Point       | Signal                                   |
| ----------- | ---------------------------------------- |
| `TP_SHUNT+` | Côté IN+ de l'amplificateur (batterie +) |
| `TP_SHUNT-` | Côté IN- de l'amplificateur (charge AGV) |
| `TP_3V3`    | Référence alimentation logique           |


---

## Mode laboratoire

Le mode labo permet de valider le firmware **sans AGV connecté** :

```
LABON
SIM 16          # Simule 16 A → alarme warning après 3 s (rouge clignote)
SIM 22          # Simule 22 A → alarme critique (rouge fixe + buzzer)
SIMTEMP 80      # Simule 80 °C PCB1 → alarme critique température
SIMTEMP2 80     # Simule 80 °C PCB2
LABOFF          # Retour aux capteurs réels (INA237, NTC, TMP126)
```

En mode labo, les valeurs simulées remplacent les lectures capteurs réelles. La **LED verte reste allumée** ; seule la **rouge** signale les alarmes. Le buzzer et le logging fonctionnent normalement.

---

## Configuration matérielle

Tous les paramètres matériels sont centralisés dans `include/board_config.h`.

> **Important :** les broches GPIO ci-dessous sont des valeurs par défaut. Elles doivent être validées et adaptées selon le schéma PCB final de l'équipe électronique.

### GPIO


| Fonction             | GPIO | Direction      |
| -------------------- | ---- | -------------- |
| I2C SDA              | 21   | Bidirectionnel |
| I2C SCL              | 22   | Bidirectionnel |
| NTC PCB 1 (ADC)      | 26   | Entrée         |
| NTC PCB 2 (ADC)      | 25   | Entrée         |
| TMP126 CS / MOSI / MISO / SCLK | 5 / 23 / 19 / 18 | SPI |
| Détection USB        | 35   | Entrée         |
| Détection BT service | 27   | Entrée         |
| Détection HT 36V     | 4    | Entrée         |
| LED verte            | 14   | Sortie         |
| LED rouge            | 15   | Sortie         |
| Buzzer (PWM)         | 13   | Sortie         |
| PSRAM CLK / CS       | 6 / 10 | SDIO partagé |
| Bouton BOOT          | 0    | Entrée         |
| Bouton RESET         | 16   | Entrée         |


### Périodes d'acquisition


| Paramètre          | Valeur  | Fichier                  |
| ------------------ | ------- | ------------------------ |
| Mesure capteurs    | 500 ms  | `MEASURE_INTERVAL_MS`    |
| Notification BLE   | 1000 ms | `BLE_NOTIFY_INTERVAL_MS` |
| Enregistrement log | 5000 ms | `LOG_INTERVAL_MS`        |


---

## Bus I2C

### Tableau des adresses


| Composant          | Adresse 7 bits | Connecteur           |
| ------------------ | -------------- | -------------------- |
| INA237AIDGST (courant)   | 0x40           | Embarqué (I2C)       |
| TMP126DCKR (ambiante)     | SPI            | Embarqué             |
| DS3231 (RTC)       | 0x68           | Embarqué (optionnel) |
| LIS3DH (vibration) | 0x18           | Grove 1              |
| SHT31 (humidité)   | 0x44           | Grove 2              |
| LoRa SX1276        | SPI (pas I2C)  | MikroBus             |


Aucun conflit d'adresse sur le bus principal. Voir `docs/i2c_address_map.md` pour le détail.

### Vérification

```
I2CSCAN
```

Résultat attendu en configuration nominale :

```
  0x40 detecte    (INA237AIDGST)
  TMP126          via SPI (ID 0x2126)
  0x68 detecte    (DS3231, si présent)
```

---

## Documentation complémentaire


| Document                                                       | Contenu                                       | Jalon   |
| -------------------------------------------------------------- | --------------------------------------------- | ------- |
| [docs/psram_wiring.md](docs/psram_wiring.md)                   | Câblage PSRAM PICO-D4, flash, diagnostic      | Prod    |
| [docs/jalon1_architecture.md](docs/jalon1_architecture.md)     | Architecture stockage, rotation, alimentation | Jalon 1 |
| [docs/jalon2_ble_shunt.md](docs/jalon2_ble_shunt.md)           | Câblage shunt IN+/IN-, GATT BLE, boot/reset   | Jalon 2 |
| [docs/jalon3_thermique.md](docs/jalon3_thermique.md)           | Placement NTC, isolation thermique            | Jalon 3 |
| [docs/guide_validation_labo.md](docs/guide_validation_labo.md) | Checklist complète de validation              | Tous    |
| [docs/i2c_address_map.md](docs/i2c_address_map.md)             | Tableau adresses I2C, compatibilité 3,3 V     | Jalon 1 |
| [ventec-ble-api/PROTOCOL.md](ventec-ble-api/PROTOCOL.md)       | Spécification BLE GATT octets / UUID          | BLE     |


---

## Dépannage

### La compilation échoue

```powershell
pip install --upgrade platformio
python -m platformio run --target clean
python -m platformio run
```

### `pio` n'est pas reconnu

Utiliser la forme complète :

```powershell
python -m platformio run
```

### PSRAM non détectée / arrêt au boot

```
E (664) spiram: SPI SRAM memory test fail. 32768/65536 writes failed
*** ARRET : PSRAM IS66WVS1M8BLL OBLIGATOIRE ***
```

Vérifications matérielles (voir `docs/psram_wiring.md`) :

1. Soudure PSRAM — surtout **SIO2 → GPIO7**, **CS → GPIO10**, **CLK → GPIO6**
2. Alimentation 3,3 V + condensateur 100 nF sur VDD PSRAM
3. Reflash propre : `pio run -e ventec_monitor -t fullclean && upload`

Pour continuer les tests sans PSRAM :

```powershell
python -m platformio run -e ventec_monitor_lab -t upload --upload-port COMx
```

### INA237AIDGST non détecté au démarrage

```
ATTENTION: INA237 non detecte (verifier I2C)
```

Vérifications :

1. `I2CSCAN` → l'adresse `0x40` doit apparaître
2. Câblage SDA (GPIO 21) et SCL (GPIO 22)
3. Alimentation 3,3 V du capteur
4. Résistances de pull-up I2C (4,7 kΩ typique)

### NTC PCB : valeurs aberrantes (-40 °C ou figées)

1. Vérifier le diviseur 10 kΩ + NTC + condensateur 100 nF
2. `LABOFF` pour désactiver la simulation
3. `STATUS` — PCB1 = GPIO 26, PCB2 = GPIO 25
4. Chauffer localement une NTC : seule la sonde concernée doit monter

### Pas de données BLE

1. Vérifier que le smartphone supporte BLE 4.0+
2. Le nom `Ventec-AGV-Monitor` doit apparaître dans nRF Connect
3. Activer les notifications sur les caractéristiques `...0002`, `...0003`, `...0004`

### Courant incorrect après calibration

1. `CALRESET` pour réinitialiser
2. Vérifier l'ampèremètre de référence
3. Re-calibrer : `CAL <valeur_exacte>`
4. Vérifier la valeur du shunt dans `board_config.h` : `SHUNT_RESISTOR_OHM = 0.0005f` (PSR400ITQFF0L50, 0,5 mΩ)

### Flash échoue (port COM)

```
Could not open COMx, the port is busy or doesn't exist
```

1. Fermer le moniteur série (`Ctrl+C`) avant l'upload
2. Débrancher/rebrancher le câble USB (câble **données**, pas charge seule)
3. Installer le driver USB-UART (CP2102 ou CH340)
4. Identifier le port : `python -m platformio device list`
5. Flasher avec le port explicite : `--upload-port COMx`
6. Maintenir BOOT enfoncé si la séquence auto DTR/RTS échoue

### Logs vides après `EXPORT`

Les mesures sont enregistrées toutes les 5 secondes. Attendre au moins 30 secondes après le démarrage avant d'exporter.

---

## Licence et auteurs

Projet développé dans le cadre du cahier des charges **Ventec Systems — Carte de monitoring AGV**.

- **Client :** Ventec Systems
- **Plateforme :** ESP32 / PlatformIO / Arduino
- **Version firmware :** 1.3

---

## Prochaines étapes

- Valider la soudure PSRAM (IS66WVS1M8BLL) et confirmer `psramFound(): oui`
- Calibrer le shunt en laboratoire (procédure `docs/guide_validation_labo.md`)
- Tester l'API BLE avec l'app Android (`ventec-ble-api/android-app/`)
- Intégrer le module LoRa MikroBus (driver SPI SX1276)
- Valider GPIO définitifs avec le schéma PCB Ventec final

