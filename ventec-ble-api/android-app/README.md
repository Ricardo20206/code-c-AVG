# Ventec Monitor — App Android minimale

Écran technicien : scan BLE, connexion à `Ventec-AGV-Monitor`, affichage live des mesures GATT.

## Ouvrir le projet

1. Lancer **Android Studio** (Hedgehog ou plus récent).
2. **File → Open** → sélectionner ce dossier `android-app`.
3. Laisser Gradle synchroniser (téléchargement des dépendances au premier lancement).
4. Brancher une tablette Android ou lancer un émulateur **avec Bluetooth** (un vrai appareil est recommandé).

## Lancer l’app

1. Flasher la carte ESP32 (firmware v1.3+).
2. Sur la tablette : activer Bluetooth et autoriser les permissions.
3. Ouvrir **Ventec Monitor**.
4. Appuyer sur **Scanner et connecter**.
5. Les valeurs s’affichent (notify ~1 Hz).

## Affichage

- Courant 36V, températures PCB1/PCB2/ambiante
- Niveau alarme, source alimentation
- Humidité, vibration, état INA237 / TMP126
- Dernier événement alarme (notify immédiat)

## Structure

```
app/src/main/java/
  com/ventec/ble/     VentecBleUuids, VentecBleParser, VentecLiveState
  com/ventec/monitor/ MainActivity, VentecBleClient
```

## Build en ligne de commande (optionnel)

```bash
cd android-app
./gradlew assembleDebug
```

APK : `app/build/outputs/apk/debug/app-debug.apk`

## Dépannage

| Problème | Solution |
|----------|----------|
| Aucun périphérique trouvé | Carte alimentée, firmware flashé, nom `Ventec-AGV-Monitor` visible dans nRF Connect |
| Permissions refusées | Paramètres → Apps → Ventec Monitor → Bluetooth / Localisation |
| Données vides | Activer mode labo sur la carte (`LABON`) pour simuler sans capteurs |

Spécification GATT : voir `../PROTOCOL.md`.
