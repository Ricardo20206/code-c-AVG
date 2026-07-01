# Tableau des adresses I2C — Ventec AGV Monitor (EXF-31)

| Composant              | Bus    | Adresse 7 bits | Adresse 8 bits | Connecteur   | Notes                          |
|------------------------|--------|----------------|----------------|--------------|--------------------------------|
| INA219 (courant)       | I2C    | 0x40           | 0x80           | Embarqué     | A0/A1 = GND                    |
| TMP117 (T° ambiante)   | I2C    | 0x48           | 0x90           | Embarqué     | ADD0 = GND                     |
| DS3231 (RTC optionnel) | I2C    | 0x68           | 0xD0           | Embarqué     | Horodatage absolu              |
| LIS3DH (vibration)     | I2C    | 0x18           | 0x30           | Grove 1      | SA0 = GND                      |
| SHT31 (humidité)       | I2C    | 0x44           | 0x88           | Grove 2      | ADDR = GND                     |
| LoRa SX1276              | SPI    | —              | —              | MikroBus     | Pas de conflit I2C             |

## Vérification des conflits

Toutes les adresses I2C sont **uniques** sur le bus principal (SDA=GPIO21, SCL=GPIO22).

- Plage utilisée : 0x18, 0x40, 0x44, 0x48, 0x68
- Aucun chevauchement détecté.

## Compatibilité niveaux logiques

Tous les composants I2C listés fonctionnent en **3,3 V**, compatible ESP32 sans adaptateur de niveau.

## Capteurs optionnels

Les capteurs Grove et le module LoRa sont détectés à l'initialisation. Le firmware continue de fonctionner si un capteur est absent.
