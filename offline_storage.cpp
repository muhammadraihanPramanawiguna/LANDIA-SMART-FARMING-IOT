
#include "offline_storage.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// LANDIA FARM - OFFLINE STORAGE V3
//
// Strategy:
// - Append-only saat enqueue.
// - Checksum untuk validasi integritas record.
// - Recovery untuk menangani interrupted write.
// - Compaction hanya ketika record mendapat ACK.
// - Event queue diprioritaskan.
// - Tidak melakukan format LittleFS otomatis.
// ============================================================

namespace {

// ------------------------------------------------------------
// FILE PATHS
// ------------------------------------------------------------

const char* EVENT_FILE = "/offline_events.log";
const char* TELEMETRY_FILE = "/offline_telemetry.log";

const char* EVENT_TEMP = "/offline_events.tmp";
const char* TELEMETRY_TEMP = "/offline_telemetry.tmp";

const char* EVENT_BACKUP = "/offline_events.bak";
const char* TELEMETRY_BACKUP = "/offline_telemetry.bak";

// ------------------------------------------------------------
// STORAGE LIMITS
// ------------------------------------------------------------

const size_t MAX_STORAGE_PERCENT = 70;
const size_t EVENT_RESERVE_PERCENT = 15;

const size_t MAX_TOPIC_LENGTH = 128;
const size_t MAX_PAYLOAD_LENGTH = 512;
const size_t MAX_RECORD_ID_LENGTH = 40;

// ID + topic + payload + checksum + separator.
const size_t MAX_LINE_LENGTH =
    MAX_RECORD_ID_LENGTH +
    MAX_TOPIC_LENGTH +
    MAX_PAYLOAD_LENGTH +
    32;

bool storageReady = false;

// ------------------------------------------------------------
// INTERNAL ENUMS
// ------------------------------------------------------------

enum class LineResult {
    NONE,
    COMPLETE,
    UNTERMINATED,
    TOO_LONG
};

enum class SearchResult {
    FOUND,
    NOT_FOUND,
    ERROR
};

// ------------------------------------------------------------
// FILE PATH HELPERS
// ------------------------------------------------------------

const char* queuePath(bool isEvent) {
    return isEvent ? EVENT_FILE : TELEMETRY_FILE;
}

const char* tempPath(bool isEvent) {
    return isEvent ? EVENT_TEMP : TELEMETRY_TEMP;
}

const char* backupPath(bool isEvent) {
    return isEvent ? EVENT_BACKUP : TELEMETRY_BACKUP;
}

// ------------------------------------------------------------
// RECORD ID GENERATION
// ------------------------------------------------------------

bool generateRecordId(String& recordId) {
    uint32_t randomA = esp_random();
    uint32_t randomB = esp_random();

    char buffer[MAX_RECORD_ID_LENGTH + 1];

    int length = snprintf(
        buffer,
        sizeof(buffer),
        "LND-%08lX%08lX",
        (unsigned long)randomA,
        (unsigned long)randomB
    );

    if (length <= 0 ||
        length >= (int)sizeof(buffer)) {
        return false;
    }

    recordId = buffer;

    return true;
}

// ------------------------------------------------------------
// INPUT VALIDATION
// ------------------------------------------------------------

bool containsInvalidCharacter(const char* value) {
    if (value == nullptr) {
        return true;
    }

    return strchr(value, '\t') != nullptr ||
           strchr(value, '\n') != nullptr ||
           strchr(value, '\r') != nullptr;
}

bool isValidRecord(const OfflineRecord& record) {
    if (record.recordId.length() == 0 ||
        record.recordId.length() > MAX_RECORD_ID_LENGTH ||
        record.topic.length() == 0 ||
        record.topic.length() > MAX_TOPIC_LENGTH ||
        record.payload.length() == 0 ||
        record.payload.length() > MAX_PAYLOAD_LENGTH) {
        return false;
    }

    if (containsInvalidCharacter(record.recordId.c_str()) ||
        containsInvalidCharacter(record.topic.c_str()) ||
        containsInvalidCharacter(record.payload.c_str())) {
        return false;
    }

    return true;
}

// ------------------------------------------------------------
// CHECKSUM - FNV-1a 32 BIT
// ------------------------------------------------------------

uint32_t calculateChecksum(
    bool isEvent,
    const String& recordId,
    const String& topic,
    const String& payload
) {
    uint32_t hash = 2166136261UL;

    auto update = [&hash](const String& value) {
        for (size_t i = 0; i < value.length(); ++i) {
            hash ^= (uint8_t)value[i];
            hash *= 16777619UL;
        }
    };

    hash ^= isEvent ? 'E' : 'T';
    hash *= 16777619UL;

    update(recordId);
    update(topic);
    update(payload);

    return hash;
}

// ------------------------------------------------------------
// RECORD SERIALIZATION
//
// V3:
// E<TAB>ID<TAB>topic<TAB>payload<TAB>checksum<NEWLINE>
// T<TAB>ID<TAB>topic<TAB>payload<TAB>checksum<NEWLINE>
// ------------------------------------------------------------

String makeRecord(const OfflineRecord& record) {
    uint32_t checksum = calculateChecksum(
        record.isEvent,
        record.recordId,
        record.topic,
        record.payload
    );

    char checksumBuffer[9];

    snprintf(
        checksumBuffer,
        sizeof(checksumBuffer),
        "%08lX",
        (unsigned long)checksum
    );

    String serialized;

    serialized.reserve(
        record.recordId.length() +
        record.topic.length() +
        record.payload.length() +
        32
    );

    serialized += record.isEvent ? "E\t" : "T\t";
    serialized += record.recordId;
    serialized += '\t';
    serialized += record.topic;
    serialized += '\t';
    serialized += record.payload;
    serialized += '\t';
    serialized += checksumBuffer;
    serialized += '\n';

    return serialized;
}

// ------------------------------------------------------------
// LEGACY RECORD PARSER
// ------------------------------------------------------------

bool parseLegacyRecord(
    const String& line,
    bool isEvent,
    OfflineRecord& record
) {
    int first = line.indexOf('\t');
    int second = line.indexOf('\t', first + 1);
    int third = line.indexOf('\t', second + 1);

    if (first < 0 || second < 0 || third < 0) {
        return false;
    }

    record.recordId = line.substring(0, first);
    record.topic = line.substring(first + 1, second);
    record.payload = line.substring(second + 1, third);
    record.isEvent = isEvent;

    return isValidRecord(record);
}

// ------------------------------------------------------------
// RECORD PARSER
// ------------------------------------------------------------

bool parseRecord(
    const String& line,
    bool expectedEvent,
    OfflineRecord& record,
    bool& legacy
) {
    legacy = false;

    if (line.length() == 0 ||
        line.length() > MAX_LINE_LENGTH) {
        return false;
    }

    // V3 record harus diawali E atau T.
    if (line[0] != 'E' &&
        line[0] != 'T') {

        legacy = true;

        return parseLegacyRecord(
            line,
            expectedEvent,
            record
        );
    }

    bool isEvent = line[0] == 'E';

    if (isEvent != expectedEvent ||
        line.length() < 3 ||
        line[1] != '\t') {
        return false;
    }

    int first = line.indexOf('\t', 2);
    int second = line.indexOf('\t', first + 1);
    int third = line.indexOf('\t', second + 1);

    if (first < 0 || second < 0 || third < 0) {
        return false;
    }

    record.recordId = line.substring(2, first);
    record.topic = line.substring(first + 1, second);
    record.payload = line.substring(second + 1, third);
    record.isEvent = isEvent;

    String checksumText = line.substring(third + 1);

    if (checksumText.length() != 8) {
        return false;
    }

    if (!isValidRecord(record)) {
        return false;
    }

    char* endPointer = nullptr;

    unsigned long storedChecksum = strtoul(
        checksumText.c_str(),
        &endPointer,
        16
    );

    if (endPointer == checksumText.c_str() ||
        *endPointer != '\0') {
        return false;
    }

    uint32_t calculatedChecksum = calculateChecksum(
        record.isEvent,
        record.recordId,
        record.topic,
        record.payload
    );

    return calculatedChecksum ==
           (uint32_t)storedChecksum;
}

// ------------------------------------------------------------
// LINE READER
// ------------------------------------------------------------

LineResult readLine(File& file, String& line) {
    line = "";

    bool foundData = false;

    while (file.available()) {
        int value = file.read();

        if (value < 0) {
            break;
        }

        foundData = true;

        if (value == '\n') {
            return LineResult::COMPLETE;
        }

        if (line.length() >= MAX_LINE_LENGTH) {
            return LineResult::TOO_LONG;
        }

        if (value != '\r') {
            line += (char)value;
        }
    }

    if (!foundData) {
        return LineResult::NONE;
    }

    return LineResult::UNTERMINATED;
}

// ------------------------------------------------------------
// APPEND RECORD
// ------------------------------------------------------------

bool appendRecord(
    const char* path,
    const String& serialized
) {
    File file = LittleFS.open(path, "a");

    if (!file) {
        Serial.println(
            "[OFFLINE] ERROR: gagal membuka file antrean."
        );

        return false;
    }

    size_t written = file.print(serialized);

    file.flush();
    file.close();

    if (written != serialized.length()) {
        Serial.println(
            "[OFFLINE] ERROR: penulisan record tidak lengkap."
        );

        return false;
    }

    return true;
}

// ------------------------------------------------------------
// QUEUE SIZE
// ------------------------------------------------------------

size_t queueSize(const char* path) {
    if (!LittleFS.exists(path)) {
        return 0;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        return 0;
    }

    size_t result = file.size();

    file.close();

    return result;
}

// ------------------------------------------------------------
// FIND RECORD
// ------------------------------------------------------------

SearchResult findRecord(
    bool isEvent,
    const String& targetId
) {
    const char* path = queuePath(isEvent);

    if (!LittleFS.exists(path)) {
        return SearchResult::NOT_FOUND;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        return SearchResult::ERROR;
    }

    SearchResult result = SearchResult::NOT_FOUND;

    String line;

    while (true) {
        LineResult lineResult = readLine(file, line);

        if (lineResult == LineResult::NONE) {
            break;
        }

        if (lineResult == LineResult::TOO_LONG) {
            result = SearchResult::ERROR;
            break;
        }

        OfflineRecord record;
        bool legacy = false;

        if (!parseRecord(
                line,
                isEvent,
                record,
                legacy
            )) {
            result = SearchResult::ERROR;
            break;
        }

        if (record.recordId == targetId) {
            if (result == SearchResult::FOUND) {
                result = SearchResult::ERROR;
                break;
            }

            result = SearchResult::FOUND;
        }

        if (lineResult == LineResult::UNTERMINATED) {
            break;
        }
    }

    file.close();

    return result;
}

// ------------------------------------------------------------
// QUEUE COMPACTION
// ------------------------------------------------------------

bool compactQueue(
    bool isEvent,
    const String& targetId
) {
    const char* path = queuePath(isEvent);
    const char* temp = tempPath(isEvent);
    const char* backup = backupPath(isEvent);

    File source = LittleFS.open(path, "r");

    if (!source) {
        return false;
    }

    File destination = LittleFS.open(temp, "w");

    if (!destination) {
        source.close();
        return false;
    }

    bool success = true;
    size_t removedCount = 0;

    String line;

    while (true) {
        LineResult lineResult = readLine(source, line);

        if (lineResult == LineResult::NONE) {
            break;
        }

        if (lineResult == LineResult::TOO_LONG) {
            success = false;
            break;
        }

        OfflineRecord record;
        bool legacy = false;

        if (!parseRecord(
                line,
                isEvent,
                record,
                legacy
            )) {
            success = false;
            break;
        }

        if (record.recordId == targetId) {
            ++removedCount;
        } else {
            String serialized = makeRecord(record);

            if (destination.print(serialized) !=
                serialized.length()) {
                success = false;
                break;
            }
        }

        if (lineResult == LineResult::UNTERMINATED) {
            break;
        }
    }

    source.close();

    destination.flush();
    destination.close();

    if (!success || removedCount != 1) {
        LittleFS.remove(temp);

        Serial.println(
            "[OFFLINE] ERROR: compaction gagal atau ID ambigu."
        );

        return false;
    }

    if (LittleFS.exists(backup) &&
        !LittleFS.remove(backup)) {
        LittleFS.remove(temp);
        return false;
    }

    if (!LittleFS.rename(path, backup)) {
        LittleFS.remove(temp);
        return false;
    }

    if (!LittleFS.rename(temp, path)) {
        LittleFS.rename(backup, path);

        Serial.println(
            "[OFFLINE] ERROR: aktivasi file hasil compaction gagal."
        );

        return false;
    }

    if (LittleFS.exists(backup) &&
        !LittleFS.remove(backup)) {
        Serial.println(
            "[OFFLINE] WARNING: backup lama belum terhapus."
        );
    }

    return true;
}

// ------------------------------------------------------------
// QUEUE RECOVERY
// ------------------------------------------------------------

bool recoverQueueFiles(bool isEvent) {
    const char* path = queuePath(isEvent);
    const char* temp = tempPath(isEvent);
    const char* backup = backupPath(isEvent);

    if (!LittleFS.exists(path) &&
        LittleFS.exists(backup)) {

        if (!LittleFS.rename(backup, path)) {
            Serial.println(
                "[OFFLINE] ERROR: gagal memulihkan backup."
            );

            return false;
        }
    }

    if (LittleFS.exists(path) &&
        LittleFS.exists(backup)) {

        if (!LittleFS.remove(backup)) {
            Serial.println(
                "[OFFLINE] WARNING: backup lama belum terhapus."
            );
        }
    }

    if (LittleFS.exists(temp)) {
        if (!LittleFS.remove(temp)) {
            Serial.println(
                "[OFFLINE] ERROR: temporary file tidak dapat dihapus."
            );

            return false;
        }
    }

    if (!LittleFS.exists(path)) {
        return true;
    }

    File source = LittleFS.open(path, "r");

    if (!source) {
        return false;
    }

    bool needsRewrite = false;
    bool success = true;

    String line;

    while (true) {
        LineResult lineResult = readLine(source, line);

        if (lineResult == LineResult::NONE) {
            break;
        }

        if (lineResult == LineResult::TOO_LONG) {
            success = false;
            break;
        }

        OfflineRecord record;
        bool legacy = false;

        if (!parseRecord(
                line,
                isEvent,
                record,
                legacy
            )) {
            if (lineResult == LineResult::UNTERMINATED) {
                needsRewrite = true;
                break;
            }

            success = false;
            break;
        }

        if (legacy ||
            lineResult == LineResult::UNTERMINATED) {
            needsRewrite = true;
        }

        if (lineResult == LineResult::UNTERMINATED) {
            break;
        }
    }

    source.close();

    if (!success) {
        Serial.println(
            "[OFFLINE] ERROR: antrean rusak. File asli dipertahankan."
        );

        return false;
    }

    if (!needsRewrite) {
        return true;
    }

    source = LittleFS.open(path, "r");

    if (!source) {
        return false;
    }

    File destination = LittleFS.open(temp, "w");

    if (!destination) {
        source.close();
        return false;
    }

    success = true;

    while (true) {
        LineResult lineResult = readLine(source, line);

        if (lineResult == LineResult::NONE) {
            break;
        }

        if (lineResult == LineResult::TOO_LONG) {
            success = false;
            break;
        }

        OfflineRecord record;
        bool legacy = false;

        if (!parseRecord(
                line,
                isEvent,
                record,
                legacy
            )) {
            if (lineResult == LineResult::UNTERMINATED) {
                break;
            }

            success = false;
            break;
        }

        String serialized = makeRecord(record);

        if (destination.print(serialized) !=
            serialized.length()) {
            success = false;
            break;
        }

        if (lineResult == LineResult::UNTERMINATED) {
            break;
        }
    }

    source.close();

    destination.flush();
    destination.close();

    if (!success) {
        LittleFS.remove(temp);

        Serial.println(
            "[OFFLINE] ERROR: migrasi antrean gagal."
        );

        return false;
    }

    if (LittleFS.exists(backup)) {
        if (!LittleFS.remove(backup)) {
            LittleFS.remove(temp);
            return false;
        }
    }

    if (!LittleFS.rename(path, backup)) {
        LittleFS.remove(temp);
        return false;
    }

    if (!LittleFS.rename(temp, path)) {
        LittleFS.rename(backup, path);

        Serial.println(
            "[OFFLINE] ERROR: aktivasi hasil recovery gagal."
        );

        return false;
    }

    if (LittleFS.exists(backup) &&
        !LittleFS.remove(backup)) {
        Serial.println(
            "[OFFLINE] WARNING: backup recovery belum terhapus."
        );
    }

    return true;
}

// ------------------------------------------------------------
// READ FIRST RECORD
// ------------------------------------------------------------

bool readFirstRecord(
    bool isEvent,
    OfflineRecord& record
) {
    const char* path = queuePath(isEvent);

    if (!LittleFS.exists(path)) {
        return false;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        Serial.println(
            "[OFFLINE] ERROR: gagal membuka antrean."
        );

        return false;
    }

    String line;

    LineResult result = readLine(file, line);

    file.close();

    if (result == LineResult::NONE) {
        return false;
    }

    if (result == LineResult::TOO_LONG) {
        Serial.println(
            "[OFFLINE] ERROR: record pertama terlalu panjang."
        );

        return false;
    }

    bool legacy = false;

    if (!parseRecord(
            line,
            isEvent,
            record,
            legacy
        )) {
        Serial.println(
            "[OFFLINE] ERROR: record pertama tidak valid."
        );

        return false;
    }

    return true;
}

// ------------------------------------------------------------
// COUNT RECORDS
// ------------------------------------------------------------

size_t countRecords(const char* path) {
    if (!LittleFS.exists(path)) {
        return 0;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        return 0;
    }

    size_t count = 0;
    String line;

    while (true) {
        LineResult result = readLine(file, line);

        if (result == LineResult::NONE ||
            result == LineResult::TOO_LONG) {
            break;
        }

        ++count;

        if (result == LineResult::UNTERMINATED) {
            break;
        }
    }

    file.close();

    return count;
}

} // namespace

// ============================================================
// PUBLIC API
// ============================================================

// ------------------------------------------------------------
// STORAGE STATUS
// ------------------------------------------------------------

bool offlineStorageIsReady() {
    return storageReady;
}

// ------------------------------------------------------------
// INITIALIZATION
// ------------------------------------------------------------

bool offlineStorageBegin() {
    storageReady = false;

    Serial.println();
    Serial.println("========== OFFLINE STORAGE INIT ==========");

    // Jangan format otomatis karena bisa menghapus data antrean.
    if (!LittleFS.begin(false)) {
        Serial.println(
            "[OFFLINE] ERROR: LittleFS gagal di-mount."
        );

        return false;
    }

    Serial.println(
        "[OFFLINE] LittleFS berhasil di-mount."
    );

    if (!recoverQueueFiles(true)) {
        LittleFS.end();
        return false;
    }

    if (!recoverQueueFiles(false)) {
        LittleFS.end();
        return false;
    }

    storageReady = true;

    Serial.printf(
        "[OFFLINE] Total: %u bytes\n",
        (unsigned int)LittleFS.totalBytes()
    );

    Serial.printf(
        "[OFFLINE] Digunakan: %u bytes\n",
        (unsigned int)LittleFS.usedBytes()
    );

    Serial.printf(
        "[OFFLINE] Event records: %u\n",
        (unsigned int)offlineStorageEventCount()
    );

    Serial.printf(
        "[OFFLINE] Telemetry records: %u\n",
        (unsigned int)offlineStorageTelemetryCount()
    );

    Serial.println(
        "[OFFLINE] LittleFS READY."
    );

    return true;
}

// ------------------------------------------------------------
// GENERATE RECORD ID
// ------------------------------------------------------------

bool offlineStorageGenerateRecordId(
    char* buffer,
    size_t bufferSize
) {
    if (!storageReady ||
        buffer == nullptr ||
        bufferSize < MAX_RECORD_ID_LENGTH + 1) {
        return false;
    }

    String recordId;

    if (!generateRecordId(recordId)) {
        return false;
    }

    if (recordId.length() + 1 > bufferSize) {
        return false;
    }

    recordId.toCharArray(buffer, bufferSize);

    return true;
}

// ------------------------------------------------------------
// ENQUEUE WITH EXPLICIT RECORD ID
// ------------------------------------------------------------

bool offlineStorageEnqueue(
    const char* recordId,
    const char* topic,
    const char* payload,
    bool isEvent
) {
    if (!storageReady ||
        recordId == nullptr ||
        topic == nullptr ||
        payload == nullptr ||
        recordId[0] == '\0' ||
        topic[0] == '\0' ||
        payload[0] == '\0') {
        return false;
    }

    if (containsInvalidCharacter(recordId) ||
        containsInvalidCharacter(topic) ||
        containsInvalidCharacter(payload)) {
        Serial.println(
            "[OFFLINE] ERROR: karakter pemisah tidak valid."
        );

        return false;
    }

    if (strlen(recordId) > MAX_RECORD_ID_LENGTH ||
        strlen(topic) > MAX_TOPIC_LENGTH ||
        strlen(payload) > MAX_PAYLOAD_LENGTH) {
        Serial.println(
            "[OFFLINE] ERROR: ukuran record melewati batas."
        );

        return false;
    }

    OfflineRecord record;

    record.recordId = recordId;
    record.topic = topic;
    record.payload = payload;
    record.isEvent = isEvent;

    if (!isValidRecord(record)) {
        Serial.println(
            "[OFFLINE] ERROR: record tidak valid."
        );

        return false;
    }

    // Pastikan ID tidak digunakan oleh record lain.
    SearchResult eventResult = findRecord(
        true,
        record.recordId
    );

    if (eventResult != SearchResult::NOT_FOUND) {
        Serial.println(
            "[OFFLINE] ERROR: recordId sudah ada atau antrean event bermasalah."
        );

        return false;
    }

    SearchResult telemetryResult = findRecord(
        false,
        record.recordId
    );

    if (telemetryResult != SearchResult::NOT_FOUND) {
        Serial.println(
            "[OFFLINE] ERROR: recordId sudah ada atau antrean telemetry bermasalah."
        );

        return false;
    }

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();

    if (total == 0) {
        return false;
    }

    const size_t overallLimit =
        total * MAX_STORAGE_PERCENT / 100;

    const size_t eventReserve =
        total * EVENT_RESERVE_PERCENT / 100;

    const size_t telemetryLimit =
        overallLimit - eventReserve;

    String serialized = makeRecord(record);

    if (used + serialized.length() > overallLimit) {
        Serial.println(
            "[OFFLINE] ERROR: batas penggunaan LittleFS tercapai."
        );

        return false;
    }

    size_t currentQueueBytes =
        queueSize(queuePath(isEvent));

    if (isEvent) {
        if (currentQueueBytes + serialized.length() >
            eventReserve) {
            Serial.println(
                "[OFFLINE] ERROR: cadangan antrean event penuh."
            );

            return false;
        }
    } else {
        if (used + serialized.length() > telemetryLimit) {
            Serial.println(
                "[OFFLINE] ERROR: batas antrean telemetry tercapai."
            );

            return false;
        }
    }

    // Append-only: tidak menulis ulang seluruh antrean.
    if (!appendRecord(
            queuePath(isEvent),
            serialized
        )) {
        return false;
    }

    Serial.printf(
        "[OFFLINE] Enqueued %s: %s\n",
        isEvent ? "EVENT" : "TELEMETRY",
        record.recordId.c_str()
    );

    return true;
}

// ------------------------------------------------------------
// LEGACY ENQUEUE
//
// Dipertahankan untuk kompatibilitas pemanggil lama.
// Integrasi MQTT berikutnya harus menggunakan overload
// dengan recordId eksplisit.
// ------------------------------------------------------------

bool offlineStorageEnqueue(
    const char* topic,
    const char* payload,
    bool isEvent
) {
    char recordId[MAX_RECORD_ID_LENGTH + 1];

    if (!offlineStorageGenerateRecordId(
            recordId,
            sizeof(recordId)
        )) {
        return false;
    }

    return offlineStorageEnqueue(
        recordId,
        topic,
        payload,
        isEvent
    );
}

// ------------------------------------------------------------
// PEEK
// ------------------------------------------------------------

bool offlineStoragePeek(OfflineRecord& record) {
    if (!storageReady) {
        return false;
    }

    // Event selalu diprioritaskan.
    if (readFirstRecord(true, record)) {
        return true;
    }

    // Jangan melewati antrean event yang rusak.
    if (LittleFS.exists(EVENT_FILE) &&
        queueSize(EVENT_FILE) > 0) {

        File eventFile = LittleFS.open(EVENT_FILE, "r");

        if (!eventFile) {
            return false;
        }

        bool hasData = eventFile.size() > 0;
        eventFile.close();

        if (hasData) {
            OfflineRecord check;
            String line;

            eventFile = LittleFS.open(EVENT_FILE, "r");

            if (!eventFile) {
                return false;
            }

            LineResult lineResult = readLine(eventFile, line);
            eventFile.close();

            bool legacy = false;

            if (lineResult == LineResult::TOO_LONG ||
                (lineResult != LineResult::NONE &&
                 !parseRecord(line, true, check, legacy))) {
                return false;
            }
        }
    }

    return readFirstRecord(false, record);
}

// ------------------------------------------------------------
// REMOVE BY ACKNOWLEDGED RECORD ID
// ------------------------------------------------------------

bool offlineStorageRemove(const char* recordId) {
    if (!storageReady ||
        recordId == nullptr ||
        recordId[0] == '\0' ||
        strlen(recordId) > MAX_RECORD_ID_LENGTH ||
        containsInvalidCharacter(recordId)) {
        return false;
    }

    String targetId = recordId;

    SearchResult eventResult = findRecord(
        true,
        targetId
    );

    if (eventResult == SearchResult::ERROR) {
        Serial.println(
            "[OFFLINE] ERROR: antrean event tidak dapat diverifikasi."
        );

        return false;
    }

    if (eventResult == SearchResult::FOUND) {
        if (!compactQueue(true, targetId)) {
            return false;
        }

        Serial.printf(
            "[OFFLINE] Event ACK removed: %s\n",
            recordId
        );

        return true;
    }

    SearchResult telemetryResult = findRecord(
        false,
        targetId
    );

    if (telemetryResult == SearchResult::ERROR) {
        Serial.println(
            "[OFFLINE] ERROR: antrean telemetry tidak dapat diverifikasi."
        );

        return false;
    }

    if (telemetryResult == SearchResult::FOUND) {
        if (!compactQueue(false, targetId)) {
            return false;
        }

        Serial.printf(
            "[OFFLINE] Telemetry ACK removed: %s\n",
            recordId
        );

        return true;
    }

    Serial.printf(
        "[OFFLINE] WARNING: record ID tidak ditemukan: %s\n",
        recordId
    );

    return false;
}

// ------------------------------------------------------------
// QUEUE STATISTICS
// ------------------------------------------------------------

size_t offlineStorageTelemetryCount() {
    if (!storageReady) {
        return 0;
    }

    return countRecords(TELEMETRY_FILE);
}

size_t offlineStorageEventCount() {
    if (!storageReady) {
        return 0;
    }

    return countRecords(EVENT_FILE);
}
