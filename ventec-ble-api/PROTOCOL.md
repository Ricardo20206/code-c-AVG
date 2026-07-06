# API BLE GATT — Ventec AGV Monitor

Spécification binaire pour l’application tablette technicien (Android).

## Périphérique

| Paramètre | Valeur |
|-----------|--------|
| Nom BLE | `Ventec-AGV-Monitor` |
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Notify live | 1 Hz (toutes les 1 s si connecté) |
| Endianness | **Little-endian** (ESP32) |

## Connexion (Android)

1. Scanner les périphériques BLE.
2. Se connecter à `Ventec-AGV-Monitor`.
3. Découvrir le service `6e400001-...`.
4. Activer les notifications (CCCD) sur les caractéristiques marquées **NOTIFY**.
5. Lire à la demande les caractéristiques **READ** (logs, historique alarmes).

## Caractéristiques

| Suffixe | UUID complet | Accès | Taille | Description |
|---------|--------------|-------|--------|-------------|
| `0002` | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 4 | Courant (A), float32 |
| `0003` | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 12 | T° PCB1, PCB2, ambiante (°C), 3× float32 |
| `0004` | `6e400004-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 1 | Niveau alarme : 0=OK, 1=warning, 2=critique |
| `0005` | `6e400005-b5a3-f393-e0a9-e50e24dcca9e` | READ, WRITE | 16 | Seuils (4× float32) |
| `0006` | `6e400006-b5a3-f393-e0a9-e50e24dcca9e` | READ | variable | Export CSV (mesures + alarmes + système) |
| `0007` | `6e400007-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 4 | Statut modules |
| `0008` | `6e400008-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 10 | Télémétrie étendue |
| `0009` | `6e400009-b5a3-f393-e0a9-e50e24dcca9e` | READ, NOTIFY | 14 | Dernier événement alarme |
| `000a` | `6e40000a-b5a3-f393-e0a9-e50e24dcca9e` | READ | variable | Historique alarmes PSRAM |

### Statut (`0007`) — 4 octets

| Index | Valeur |
|-------|--------|
| 0 | Source alim : 0=inconnu, 1=HT-36V, 2=BT service, 3=USB-C |
| 1 | Grove vibration présent : 0/1 |
| 2 | Grove humidité présent : 0/1 |
| 3 | MikroBus LoRa présent : 0/1 |

### Télémétrie (`0008`) — 10 octets

| Champ | Type | Note |
|-------|------|------|
| humidity_pct | float32 | -1 si capteur absent |
| vibration_mg | float32 | -1 si capteur absent |
| sensor_flags | uint8 | bit0=INA237, bit1=TMP126, bit2=vib, bit3=hum, bit4=LoRa |
| mode_flags | uint8 | bit0=mode labo actif |

### Seuils (`0005`) — écriture 16 octets

```
[float32] current_warn_a    (défaut 15.0)
[float32] current_alarm_a   (défaut 20.0)
[float32] temp_alarm_c      (défaut 75.0)
[float32] current_drift_a   (défaut 0.5)
```

Persistés en NVS sur la carte.

### Événement alarme (`0009`) — 14 octets, NOTIFY immédiat

| Champ | Type |
|-------|------|
| timestamp_ms | uint32 |
| type | uint8 (1=courant, 2=temp PCB, 3=dérive) |
| level | uint8 (1=warning, 2=critique) |
| trigger_value | float32 |
| threshold | float32 |

### Historique alarmes (`000a`) — lecture

```
[uint8] count (0–50)
[count × 14 octets] AlarmRecord (même layout que 0009)
```

## SDK Android

Fichiers Kotlin dans `android/` :

- `VentecBleUuids.kt` — constantes UUID
- `VentecBleParser.kt` — décodage little-endian

Permissions Android 12+ : `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`.

## Test sans app custom

Utiliser **nRF Connect** : activer NOTIFY sur `0002`, `0003`, `0004`, `0007`, `0008`, `0009`.
