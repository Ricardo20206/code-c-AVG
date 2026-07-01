# Jalon 1 — Architecture stockage et alimentation

## 1. Architecture de stockage (EXF-15 à EXF-18)

### Structure des enregistrements

| Fichier LittleFS | Type | Taille unitaire | Capacité max | Contenu |
|------------------|------|-----------------|--------------|---------|
| `/meas.bin` | `MeasurementRecord` | 28 octets | 2000 | Courant, 3 températures, humidité, alarme, source |
| `/alarms.bin` | `AlarmRecord` | 20 octets | 200 | Type, niveau, valeur déclenchement, seuil |
| `/system.bin` | `SystemEventRecord` | 9 octets | 100 | Boot, changement alim, connexion technicien |

### Volume estimé

- Mesures : 2000 × 28 = **56 Ko**
- Alarmes : 200 × 20 = **4 Ko**
- Système : 100 × 9 = **~1 Ko**
- **Total : ~61 Ko** (compatible partition LittleFS ESP32)

### Politique de rotation (EXF-18)

Algorithme **ring buffer** dans `data_logger.cpp` :
- Tant que la capacité n'est pas atteinte : ajout séquentiel
- Mémoire pleine : décalage des N-1 entrées les plus récentes, écrasement de la plus ancienne
- Garantit la continuité d'enregistrement sans intervention technicien

### Tampon PSRAM alarmes (EXF-25)

50 dernières alarmes conservées en PSRAM pour accès rapide BLE, en complément de LittleFS.

---

## 2. Logique de commutation alimentation (EXF-03)

> Partie **matérielle** — le firmware ne gère pas la priorité, seulement la **détection**.

| Priorité | Source | GPIO détection | Usage |
|----------|--------|----------------|-------|
| 1 | USB-C 5V | GPIO 32 | Reprogrammation / extraction logs |
| 2 | BT service 2,7–10V | GPIO 33 | Intervention batterie principale |
| 3 | HT 36V | GPIO 25 | Exploitation normale AGV |

Le firmware enregistre chaque changement de source dans `/system.bin` (EXF-17).

### Scénarios validés (aucune double source active)

| USB | BT | HT | Source active |
|-----|----|----|---------------|
| 1 | x | x | USB-C |
| 0 | 1 | x | BT Service |
| 0 | 0 | 1 | HT 36V |
| 0 | 0 | 0 | HT 36V (défaut) |

La commutation est assurée par le circuit ORing matériel ; le firmware ne peut pas activer deux sources.

---

## 3. Tableau I2C (EXF-31)

Voir `docs/i2c_address_map.md`.

Commande firmware : `I2CSCAN` via USB pour validation terrain.

---

## 4. Commande de vérification

```
STORAGE    → statistiques stockage
GPIO       → état détection alimentation
I2CSCAN    → scan bus I2C
```
