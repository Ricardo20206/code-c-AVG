# Jalon 3 — Positionnement sondes NTC et isolation thermique

## 1. Sondes NTC PCB (EXF-11 à EXF-14)

### Chaîne de mesure

```
3,3V ──[R série 10kΩ]──┬── NTC 10kΩ ── GND
                        │
                   Condensateur 100nF ── GND
                        │
                   GPIO ADC (34 ou 35)
```

- Filtrage RC : condensateur 100 nF en parallèle sur l'entrée ADC (EXF-12)
- Diviseur résistif 10kΩ / NTC 10kΩ @ 25°C
- Lecture via formule Steinhart-Hart simplifiée (β = 3950)

### Placement recommandé sur PCB (face TOP)

| Sonde | Zone | Justification |
|-------|------|---------------|
| **NTC1 (GPIO 34)** | Près du régulateur 3,3V / zone alimentation | Détecte surchauffe convertisseurs |
| **NTC2 (GPIO 35)** | Près du connecteur shunt / INA219 | Détecte échauffement mesure courant |

### Isolation thermique (EXF-14)

```
┌─────────────────────────────────────┐
│  ZONE CHAUDE          ZONE FROIDE   │
│  (régulateur,         (NTC mesure,  │
│   MOSFET ORing)        connecteurs) │
│         │                    │      │
│    [coupe PCB]          [NTC2]      │
│    [via stitching]                  │
│                              [NTC1] │
└─────────────────────────────────────┘
```

**Mesures d'isolation :**
- Entre zone chaude (régulateur en charge) et NTC : **> 5 mm** avec coupure PCB
- Pas de cuivre massif entre source de chaleur et sonde
- Vias de stitching thermique **loin** des NTC

### Validation firmware

```
STATUS              # Lire T° PCB1 et PCB2
SIMTEMP 80          # Simuler surchauffe (mode labo LABON)
                    # → alarme critique si > 75°C
```

---

## 2. Capteur ambiant (EXF-13)

- **TMP117** à l'adresse I2C 0x48
- Position : centre du châssis AGV, à l'écart des flux d'air chaud des moteurs
- Mesure la température **à l'intérieur du carter**, pas la température extérieure

---

## 3. Démonstration isolation thermique

### Protocole de test

1. Placer la carte sur banc thermique à 25°C
2. Appliquer source de chaleur (résistance 5W) sur régulateur 3,3V
3. Enregistrer T° NTC1, NTC2 et ambiante toutes les 5 s (`EXPORT`)
4. **Critère** : écart NTC zone chaude vs NTC zone froide > 10°C à stabilisation
5. **Critère alarme** : déclenchement à 75°C sur la sonde la plus chaude uniquement

### Résultats attendus

| Phase | NTC1 (zone alim) | NTC2 (zone shunt) | Ambiante |
|-------|------------------|-------------------|----------|
| Repos | ~25°C | ~25°C | ~25°C |
| Charge 15A | ~45°C | ~35°C | ~30°C |
| Surchauffe simulée | > 75°C → alarme | < 60°C | ~35°C |

---

## 4. Compromis mono-face (EXF-36)

| Contrainte | Compromis réalisé |
|------------|-------------------|
| Composants face TOP uniquement | NTC côté composants, pas sous PCB |
| Zone 70×50 mm | INA219 + ESP32 + régulateur compacts |
| Accessibilité USB/LEDs | Connecteurs sur bord court carte |
| Routage thermique | NTC1 éloigné des zones de puissance |
