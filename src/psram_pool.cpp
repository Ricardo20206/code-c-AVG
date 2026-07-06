#include "psram_pool.h"
#include "psram_config.h"
#include <Arduino.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <cstring>

PsramPool g_psram;

static bool psramProbeAlloc() {
    void* probe = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
    if (!probe) return false;
    uint8_t* p = static_cast<uint8_t*>(probe);
    for (size_t i = 0; i < 4096; ++i) {
        p[i] = static_cast<uint8_t>(i & 0xFF);
    }
    bool ok = true;
    for (size_t i = 0; i < 4096; ++i) {
        if (p[i] != static_cast<uint8_t>(i & 0xFF)) {
            ok = false;
            break;
        }
    }
    heap_caps_free(probe);
    return ok;
}

static void psramFatalHalt() {
    Serial.println();
    Serial.println("*** ARRET : PSRAM IS66WVS1M8BLL OBLIGATOIRE ***");
    Serial.println("Carte Ventec = ESP32-PICO-D4 + IS66WVS1M8BLL");
    Serial.printf("  CLK=GPIO%d CS=GPIO%d (voir psram_config.h)\n",
                  VENTEC_PSRAM_GPIO_CLK, VENTEC_PSRAM_GPIO_CS);
    Serial.println("Verifiez :");
    Serial.println("  - Flash env ventec_monitor (pas no_psram)");
    Serial.println("  - Soudure PSRAM (SIO2->GPIO7, CS->GPIO10, CLK->GPIO6)");
    Serial.println("  - Alimentation PSRAM 3.3 V + condensateur 100 nF");
    Serial.println("  - pio run -e ventec_monitor -t fullclean && upload");
    Serial.println("  - Mode labo (sans arret) : pio run -e ventec_monitor_lab -t upload");
    while (true) {
        delay(2000);
    }
}

bool PsramPool::allocBuffers() {
    const uint32_t caps = _usingPsram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;

    _alarms = (AlarmRecord*)heap_caps_calloc(
        PSRAM_ALARM_MAX, sizeof(AlarmRecord), caps);
    _measRing = (MeasurementRecord*)heap_caps_calloc(
        PSRAM_MEAS_RING_SIZE, sizeof(MeasurementRecord), caps);

    if (!_alarms || !_measRing) {
        Serial.println("  ERREUR: allocation tampons");
        if (_alarms) heap_caps_free(_alarms);
        if (_measRing) heap_caps_free(_measRing);
        _alarms = nullptr;
        _measRing = nullptr;
        return false;
    }
    return true;
}

bool PsramPool::begin() {
    _usingPsram = false;
    _active = false;
    _totalBytes = ESP.getPsramSize();

    const bool halPsram = psramFound();
    const size_t spiramHeap = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    Serial.printf("PSRAM %s : init (PICO-D4, CLK=GPIO%d CS=GPIO%d)\n",
                  VENTEC_PSRAM_CHIP_NAME,
                  VENTEC_PSRAM_GPIO_CLK,
                  VENTEC_PSRAM_GPIO_CS);
    Serial.printf("  psramFound()     : %s\n", halPsram ? "oui" : "non");
    Serial.printf("  ESP.getPsramSize : %u Ko\n", _totalBytes / 1024);
    Serial.printf("  heap SPIRAM      : %u Ko\n", spiramHeap / 1024);

    if (!halPsram || spiramHeap == 0) {
        Serial.println("  ERREUR: PSRAM non initialisee par le SDK");
#if defined(VENTEC_PSRAM_REQUIRED) && VENTEC_PSRAM_REQUIRED
        psramFatalHalt();
#else
        Serial.println("  MODE LABO : continuation sans PSRAM (RAM interne)");
        _usingPsram = false;
        _totalBytes = 0;
        if (!allocBuffers()) {
            Serial.println("  ERREUR: allocation tampons RAM interne");
            return false;
        }
        _active = true;
        Serial.printf("  Tampon alarmes   : %u entrees (RAM interne)\n", PSRAM_ALARM_MAX);
        Serial.printf("  Anneau mesures   : %u entrees (RAM interne)\n", PSRAM_MEAS_RING_SIZE);
        return true;
#endif
        return false;
    }

    if (!psramProbeAlloc()) {
        Serial.println("  ERREUR: test lecture/ecriture PSRAM");
#if defined(VENTEC_PSRAM_REQUIRED) && VENTEC_PSRAM_REQUIRED
        psramFatalHalt();
#endif
        return false;
    }

    _usingPsram = true;
    _totalBytes = spiramHeap > 0 ? spiramHeap : _totalBytes;
    Serial.println("  Test allocation PSRAM : OK");

    if (!allocBuffers()) {
#if defined(VENTEC_PSRAM_REQUIRED) && VENTEC_PSRAM_REQUIRED
        psramFatalHalt();
#endif
        return false;
    }

    _active = true;
    Serial.printf("  Stockage tampons : PSRAM (%u Ko libres)\n", freeBytes() / 1024);
    Serial.printf("  Tampon alarmes   : %u entrees\n", PSRAM_ALARM_MAX);
    Serial.printf("  Anneau mesures   : %u entrees\n", PSRAM_MEAS_RING_SIZE);

    if (_totalBytes < VENTEC_PSRAM_NOMINAL_BYTES / 2) {
        Serial.println("  ATTENTION: taille PSRAM < 512 Ko (attendu 1 Mo)");
    }
    return true;
}

size_t PsramPool::freeBytes() const {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void PsramPool::pushAlarm(const AlarmRecord& rec) {
    if (!_alarms || _alarmCount >= PSRAM_ALARM_MAX) return;
    _alarms[_alarmCount++] = rec;
}

void PsramPool::pushMeasurement(const MeasurementRecord& rec) {
    if (!_measRing) return;
    _measRing[_measHead] = rec;
    _measHead = (_measHead + 1) % PSRAM_MEAS_RING_SIZE;
    if (_measCount < PSRAM_MEAS_RING_SIZE) {
        _measCount++;
    }
}

size_t PsramPool::copyAlarms(AlarmRecord* out, size_t maxCount) const {
    if (!out || maxCount == 0 || !_alarms) return 0;
    size_t n = _alarmCount < maxCount ? _alarmCount : maxCount;
    for (size_t i = 0; i < n; ++i) {
        out[i] = _alarms[i];
    }
    return n;
}

void PsramPool::printStatus() const {
    Serial.println("--- PSRAM Ventec ---");
    Serial.printf("  Puce reference : %s\n", VENTEC_PSRAM_CHIP_NAME);
    Serial.printf("  MCU            : ESP32-PICO-D4\n");
    Serial.printf("  Active         : %s\n", _active ? "OUI" : "NON");
    Serial.printf("  Backend        : %s\n",
                  _usingPsram ? "PSRAM" : (_active ? "RAM interne" : "AUCUN"));
    Serial.printf("  Taille SPIRAM  : %u Ko\n", _totalBytes / 1024);
    Serial.printf("  Libre SPIRAM   : %u Ko\n", freeBytes() / 1024);
    Serial.printf("  Alarmes        : %u / %u\n", _alarmCount, PSRAM_ALARM_MAX);
    Serial.printf("  Mesures anneau : %u / %u\n", _measCount, PSRAM_MEAS_RING_SIZE);
}

size_t ventecGetAlarmHistory(AlarmRecord* out, size_t maxCount) {
    return g_psram.copyAlarms(out, maxCount);
}
