#ifndef OFFLINE_STORAGE_H
#define OFFLINE_STORAGE_H

#include <Arduino.h>

// ============================================================
// OFFLINE RECORD - LANDIA FARM ESP32 LITTLEFS QUEUE
// ============================================================

struct OfflineRecord {
    String recordId;
    String topic;
    String payload;
    bool isEvent;
};

// ============================================================
// STORAGE INITIALIZATION
// ============================================================

bool offlineStorageBegin();
bool offlineStorageIsReady();

// ============================================================
// RECORD ID
// ============================================================

bool offlineStorageGenerateRecordId(
    char* buffer,
    size_t bufferSize
);

// ============================================================
// ENQUEUE
// ============================================================

bool offlineStorageEnqueue(
    const char* recordId,
    const char* topic,
    const char* payload,
    bool isEvent
);

// API lama untuk kompatibilitas.
bool offlineStorageEnqueue(
    const char* topic,
    const char* payload,
    bool isEvent
);

// ============================================================
// QUEUE ACCESS
// ============================================================

// Membaca antrean telemetry saja; tidak menghapus record.
bool offlineStoragePeekTelemetry(OfflineRecord& record);

// Membaca antrean event saja; tidak menghapus record.
bool offlineStoragePeekEvent(OfflineRecord& record);

// API lama: event diprioritaskan untuk kompatibilitas.
bool offlineStoragePeek(OfflineRecord& record);

// ============================================================
// ACKNOWLEDGED RECORD REMOVAL
// ============================================================

bool offlineStorageRemove(const char* recordId);

// ============================================================
// QUEUE STATISTICS
// ============================================================

size_t offlineStorageTelemetryCount();
size_t offlineStorageEventCount();

#endif // OFFLINE_STORAGE_H
