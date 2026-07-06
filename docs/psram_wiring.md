# Câblage PSRAM — Carte Ventec AGV Monitor

Référence puce : **ISSI IS66WVS1M8BLL-104NLI** (1 Mo, quad SPI, 3.3 V)  
MCU : **ESP32-PICO-D4** (flash 4 Mo intégrée)

## Correspondance schéma ↔ GPIO ↔ firmware

| PSRAM U102 | Signal | ESP32-PICO-D4 | GPIO | Config SDK |
|------------|--------|---------------|------|------------|
| Pin 1 | CE# | SD_DATA_3 (broche 29) | **10** | `CONFIG_SPIRAM_CS_IO=10` |
| Pin 2 | SO / SIO1 | GPIO17 (broche 27) | **17** | (bus quad partagé) |
| Pin 3 | SIO2 | SD_DATA_0 (broche 32) | **7** | (bus quad partagé) |
| Pin 5 | SI / SIO0 | SD_DATA_1 (broche 33) | **8** | (bus quad partagé) |
| Pin 6 | CLK | SD_CLK (broche 31) | **6** | `CONFIG_SPIRAM_CLK_IO=6` |
| Pin 7 | SIO3 | SD_CMD (broche 30) | **11** | (bus quad partagé) |
| Pin 8 | VDD | 3V3 | — | alimentation 3.3 V |

> **Important** : les options `CONFIG_SPIRAM_CLK_IO` et `CONFIG_SPIRAM_CS_IO` utilisent le **numéro GPIO** (6, 10), pas le numéro de broche du boîtier (31, 29).

## Fichiers de configuration firmware

| Fichier | Rôle |
|---------|------|
| `sdkconfig.defaults` | Référence GPIO (Arduino utilise le SDK précompilé PICO : CLK=6, CS=10) |
| `boards/ventec_agv_monitor.json` | ESP32-PICO-D4, flash DIO, `dio_qspi` |
| `platformio.ini` | `board_build.psram = enabled`, env `ventec_monitor` |
| `include/psram_config.h` | Référence puce + tableau GPIO |

## Flash

```powershell
python -m platformio run -e ventec_monitor -t fullclean
python -m platformio run -e ventec_monitor -t upload --upload-port COMx
```

## Vérification série

```
PSRAM IS66WVS1M8BLL-104NLI : init (PICO-D4, CLK=GPIO6 CS=GPIO10)
  psramFound()     : oui
  ESP.getPsramSize : 1024 Ko
```

Commande : `PSRAM`
