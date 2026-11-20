
#ifndef OFFLINE_STORAGE_H
#define OFFLINE_STORAGE_H

#include <Arduino.h>

// ============================================================
// OFFLINE RECORD
// LANDIA FARM - ESP32 LITTLEFS QUEUE
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

// Membuat ID unik untuk satu record.
bool offlineStorageGenerateRecordId(
    char* buffer,
    size_t bufferSize
);

// ============================================================
// ENQUEUE
// ============================================================

// Menyimpan record dengan ID yang sudah dibuat pemanggil.
// recordId harus sama dengan recordId di payload JSON.
bool offlineStorageEnqueue(
    const char* recordId,
    const char* topic,
    const char* payload,
    bool isEvent
);

// API lama dipertahankan untuk kompatibilitas.
bool offlineStorageEnqueue(
    const char* topic,
    const char* payload,
    bool isEvent
);

// ============================================================
// QUEUE ACCESS
// ============================================================

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

#endif
