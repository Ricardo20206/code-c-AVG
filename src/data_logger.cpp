#include "data_logger.h"
#include "board_config.h"
#include <FS.h>
#include <LittleFS.h>

DataLogger g_logger;

namespace {

template<typename T>
void appendRingFile(const char* path, const T& item, size_t maxCount, size_t& counter, bool ready) {
    if (!ready) return;

    File f = LittleFS.open(path, "r+");
    if (!f) f = LittleFS.open(path, "w");
    if (!f) return;

    size_t recordSize = sizeof(T);
    size_t count = f.size() / recordSize;

    if (count >= maxCount) {
        size_t shiftBytes = (maxCount - 1) * recordSize;
        std::vector<uint8_t> buf(shiftBytes);
        f.seek(recordSize);
        f.read(buf.data(), shiftBytes);
        f.seek(0);
        f.write(buf.data(), shiftBytes);
        f.write((const uint8_t*)&item, recordSize);
        counter = maxCount;
    } else {
        f.seek(count * recordSize);
        f.write((const uint8_t*)&item, recordSize);
        counter = count + 1;
    }
    f.close();
}

template<typename T>
std::vector<T> readLastFromFile(const char* path, size_t n, bool ready) {
    std::vector<T> result;
    if (!ready) return result;

    File f = LittleFS.open(path, "r");
    if (!f) return result;

    size_t recordSize = sizeof(T);
    size_t total = f.size() / recordSize;
    size_t start = (total > n) ? total - n : 0;
    f.seek(start * recordSize);

    for (size_t i = start; i < total; ++i) {
        T item{};
        if (f.read((uint8_t*)&item, recordSize) == recordSize) {
            result.push_back(item);
        }
    }
    f.close();
    return result;
}

} // namespace

bool DataLogger::begin() {
    _ready = LittleFS.begin(true);
    return _ready;
}

void DataLogger::logMeasurement(const MeasurementRecord& rec) {
    appendRingFile(LOG_FILE_MEASUREMENTS, rec, LOG_MAX_MEASUREMENTS, _measCount, _ready);
}

void DataLogger::logAlarm(const AlarmRecord& rec) {
    appendRingFile(LOG_FILE_ALARMS, rec, LOG_MAX_ALARMS, _alarmCount, _ready);
}

void DataLogger::logSystemEvent(const SystemEventRecord& ev) {
    appendRingFile(LOG_FILE_SYSTEM, ev, LOG_MAX_SYSTEM_EVENTS, _sysCount, _ready);
}

std::vector<MeasurementRecord> DataLogger::getLastMeasurements(size_t n) const {
    return readLastFromFile<MeasurementRecord>(LOG_FILE_MEASUREMENTS, n, _ready);
}

std::vector<AlarmRecord> DataLogger::getLastAlarms(size_t n) const {
    return readLastFromFile<AlarmRecord>(LOG_FILE_ALARMS, n, _ready);
}

std::vector<SystemEventRecord> DataLogger::getLastSystemEvents(size_t n) const {
    return readLastFromFile<SystemEventRecord>(LOG_FILE_SYSTEM, n, _ready);
}

String DataLogger::exportAllCsv() const {
    String csv = "type,timestamp_ms,v1,v2,v3,v4\n";

    for (const auto& m : getLastMeasurements(LOG_MAX_MEASUREMENTS)) {
        csv += "MEAS," + String(m.timestamp_ms) + "," +
               String(m.current_a, 2) + "," +
               String(m.temp_pcb1_c, 1) + "," +
               String(m.temp_pcb2_c, 1) + "," +
               String(m.temp_ambient_c, 1) + "\n";
    }
    for (const auto& a : getLastAlarms(LOG_MAX_ALARMS)) {
        csv += "ALARM," + String(a.timestamp_ms) + "," +
               String((int)a.type) + "," +
               String((int)a.level) + "," +
               String(a.trigger_value, 2) + "," +
               String(a.threshold, 2) + "\n";
    }
    for (const auto& s : getLastSystemEvents(LOG_MAX_SYSTEM_EVENTS)) {
        csv += "SYS," + String(s.timestamp_ms) + "," +
               String((int)s.type) + "," +
               String(s.param) + ",0,0\n";
    }
    return csv;
}

void DataLogger::clearAll() {
    if (!_ready) return;
    LittleFS.remove(LOG_FILE_MEASUREMENTS);
    LittleFS.remove(LOG_FILE_ALARMS);
    LittleFS.remove(LOG_FILE_SYSTEM);
    _measCount = _alarmCount = _sysCount = 0;
}
