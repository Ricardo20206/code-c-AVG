# Guide de validation laboratoire (EXF-10)

## Prérequis matériel

- [ ] Carte Ventec AGV Monitor alimentée (36V ou USB-C)
- [ ] Câble USB-C pour commandes série
- [ ] Ampèremètre de référence (classe 1% ou mieux)
- [ ] Alimentation 36V / 10A avec résistances de charge
- [ ] Multimètre pour points de test TP_SHUNT+/-
- [ ] Smartphone avec nRF Connect (test BLE)
- [ ] PlatformIO installé pour flash firmware

## Installation PlatformIO

```powershell
pip install platformio
cd "c:\Users\mberi\Desktop\code projet ESP32"
pio run -t upload
pio device monitor
```

## Checklist validation

### Étape 1 — Démarrage et bus I2C

| # | Action | Commande | Résultat attendu |
|---|--------|----------|------------------|
| 1 | Flash firmware | `pio run -t upload` | Succès compilation |
| 2 | Ouvrir moniteur | `pio device monitor` | Message "Pret. Tapez HELP" |
| 3 | Scanner I2C | `I2CSCAN` | 0x40 (INA219), 0x48 (TMP117) détectés |
| 4 | État GPIO alim | `GPIO` | HT=1 en exploitation normale |

### Étape 2 — Mesure courant (EXF-07 à EXF-10)

| # | Action | Commande | Résultat attendu |
|---|--------|----------|------------------|
| 5 | Valeur brute | `RAW` | Courant affiché en A |
| 6 | Appliquer 10A charge | multimètre | Référence = 10,0 A |
| 7 | Calibrer | `CAL 10.0` | "OK: calibre ref=10.00" |
| 8 | Vérifier | `STATUS` | Courant ≈ 10,0 A (±0,5 A) |
| 9 | Tester 0A | débrancher charge | Courant < 0,5 A |
| 10 | Tester 25A | charge max | Alarme critique après 3 s |

### Étape 3 — Températures (EXF-11 à EXF-13)

| # | Action | Commande | Résultat attendu |
|---|--------|----------|------------------|
| 11 | Lire températures | `STATUS` | PCB1, PCB2, ambiante cohérentes |
| 12 | Simuler surchauffe | `LABON` puis `SIMTEMP 80` | Alarme critique, LED rouge |

### Étape 4 — Logging et export (EXF-15 à EXF-21)

| # | Action | Commande | Résultat attendu |
|---|--------|----------|------------------|
| 13 | Stats stockage | `STORAGE` | Volumes affichés |
| 14 | Attendre 30 s | — | Mesures enregistrées |
| 15 | Export CSV | `EXPORT` | Fichier CSV avec lignes MEAS |

### Étape 5 — BLE (EXF-19, EXF-23)

| # | Action | Outil | Résultat attendu |
|---|--------|-------|------------------|
| 16 | Scanner BLE | nRF Connect | `Ventec-AGV-Monitor` visible |
| 17 | Notifications | char ...0002 | Courant mis à jour chaque seconde |
| 18 | Modifier seuils | écrire ...0005 | `THRESH` reflète les nouvelles valeurs |

### Étape 6 — Alertes locales (EXF-24 à EXF-26)

| # | Action | Résultat attendu |
|---|--------|------------------|
| 19 | Fonctionnement normal | LED verte allumée |
| 20 | `SIM 16` (LABON) | LED rouge clignote après 3 s |
| 21 | `SIM 22` (LABON) | LED rouge + buzzer continu |

## Rapport de test

À compléter après validation :

| Test | Date | Opérateur | Résultat | Notes |
|------|------|-----------|----------|-------|
| I2C scan | | | OK / KO | |
| Calibration 10A | | | OK / KO | |
| Calibration 20A | | | OK / KO | |
| Alarme courant | | | OK / KO | |
| Alarme température | | | OK / KO | |
| Export logs USB | | | OK / KO | |
| BLE notify | | | OK / KO | |
| LED / buzzer | | | OK / KO | |
