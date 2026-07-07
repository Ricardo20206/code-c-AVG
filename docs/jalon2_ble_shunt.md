# Jalon 2 — Câblage shunt, BLE et boot/reset

## 1. Câblage du shunt (EXF-07 à EXF-10)

### Orientation IN+ / IN- (sens du courant)

```
Batterie 36V (+) ──► Charge AGV ──► Batterie 36V (-)
                         │
              [Shunt PSR400ITQFF0L50 0.5mΩ]
                         │
              IN+ (côté batterie +)
              IN- (côté charge)
                         │
                    [INA237]
```

- **IN+** : côté **haut** du shunt (vers le pôle + batterie / entrée courant)
- **IN-** : côté **bas** du shunt (vers la charge AGV / sortie courant)
- Le courant positif (décharge batterie) produit une tension positive sur IN+ par rapport à IN-

### Composant shunt

| Paramètre | Valeur |
|-----------|--------|
| Référence | **ROHM PSR400ITQFF0L50** |
| Résistance | **0,5 mΩ** (±1 %) |
| Puissance | 4 W |
| Norme | AEC-Q200 |
| Format | 10×5,2 mm (3921) |
| `SHUNT_RESISTOR_OHM` | `0.0005f` |

### Plage et précision

| Paramètre | Valeur |
|-----------|--------|
| Plage courant | 0 – 30 A |
| Tension shunt @ 30 A | ~15 mV |
| Résolution INA237 (shunt) | 5 µV (plage normale) |
| Résolution affichée | ~0,1 A (après calibration) |
| Détection dérive | 0,5 A (EXF-09) |

**Tensions shunt attendues :**

| Courant | Tension |
|---------|---------|
| 1 A | 0,5 mV |
| 10 A | 5 mV |
| 20 A | 10 mV |
| 30 A | 15 mV |

### Validation laboratoire (EXF-10)

**Matériel requis :**
- Alimentation 36V simulée
- Résistances de charge (ex. 3,6Ω → 10A, 1,8Ω → 20A)
- Ampèremètre de référence sur TP_SHUNT
- Multimètre sur TP_SHUNT+ / TP_SHUNT-

**Procédure firmware :**

```
LABOFF                 # Capteurs réels
RAW                    # Courant brut + tension shunt (mV) + I=V/R
CAL 10.0               # Calibrer avec 10A mesurés au multimètre
STATUS                 # Vérifier courant et shunt (~5 mV @ 10 A)
SIM 25.0               # Tester alarme sans AGV (LABON)
```

**Points de test PCB :**
- `TP_SHUNT+` : côté IN+ amplificateur
- `TP_SHUNT-` : côté IN-
- `TP_3V3` : référence alimentation logique

---

## 2. Architecture BLE GATT (EXF-19, EXF-23)

### Service principal

| Élément | UUID | Propriétés |
|---------|------|------------|
| Service Ventec Monitor | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Primaire |

### Caractéristiques

| Nom | UUID suffixe | Type | Accès | Description |
|-----|-------------|------|-------|-------------|
| Courant | `...0002` | float32 | READ, NOTIFY | Courant instantané (A) |
| Températures | `...0003` | float32[3] | READ, NOTIFY | PCB1, PCB2, ambiante (°C) |
| Alarme | `...0004` | uint8 | READ, NOTIFY | 0=none, 1=warning, 2=critical |
| Seuils | `...0005` | struct 16B | READ, WRITE | warn_A, alarm_A, temp_C, drift_A |
| Logs | `...0006` | string CSV | READ | Export historique |
| Statut | `...0007` | uint8[4] | READ, NOTIFY | source, vib, hum, lora |
| Télémétrie | `...0008` | 10 B | READ, NOTIFY | humidité, vibration, flags capteurs |
| Alarme event | `...0009` | 14 B | READ, NOTIFY | dernier événement alarme (immédiat) |
| Historique alarmes | `...000a` | variable | READ | tampon PSRAM (count + records) |

### Test avec nRF Connect

1. Scanner → connecter à `Ventec-AGV-Monitor`
2. Ouvrir le service `6e400001-...`
3. Activer les notifications sur `...0002`, `...0003`, `...0004`, `...0007`, `...0008`, `...0009`
4. Écrire les seuils sur `...0005` (16 octets, 4 floats)
5. Lire `...0006` pour l'export CSV
6. Lire `...000a` pour l'historique alarmes PSRAM
7. SDK tablette : dossier `ventec-ble-api/` (Kotlin + PROTOCOL.md)

### Antenne BLE (EXF-20)

Recommandations hardware (hors firmware) :
- Plan de masse évité sous l'antenne PCB
- Réseau π de filtrage sur l'alimentation RF
- Positionnement : bord de carte, face TOP, zone sans métal du carter

---

## 3. Automatisation boot/reset (EXF-32 à EXF-34)

### Principe

Circuit matériel (DTR/RTS via USB-UART ou pont logique) :
- **DTR** → GPIO EN (reset)
- **RTS** → GPIO 0 (boot)

### Table de vérité

| DTR | RTS | GPIO0 | EN | Mode ESP32 |
|-----|-----|-------|----|------------|
| 0 | 1 | 0 | 1 | **Download (flash)** |
| 1 | 1 | 1 | 1 | Exécution normale |
| x | 0→1 | 1→0→1 | pulse | Reset simple |

### Fonctionnement normal (pas de flash)

Quand aucun outil de flash n'est connecté, DTR/RTS restent inactifs → GPIO0 et EN tirés haut par pull-up → exécution firmware normale.

### Procédure technicien (< 5 min, EXF-34)

1. Brancher USB-C
2. Ouvrir PlatformIO / esptool : `pio run -t upload`
3. L'outil pilote automatiquement DTR/RTS
4. Flash terminé en ~30–60 s

Boutons BOOT/RESET (GPIO 0 / GPIO 16) restent accessibles en secours manuel (EXF-33).
