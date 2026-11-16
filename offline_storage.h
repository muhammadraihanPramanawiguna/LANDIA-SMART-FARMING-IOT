
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
// ENQUEUE
// ============================================================

// Menyimpan record ke antrean LittleFS.
// Record ID akan dibuat dan disimpan oleh implementasi storage.

bool offlineStorageEnqueue(
  const char* topic,
  const char* payload,
  bool isEvent
);

// ============================================================
// QUEUE ACCESS
// ============================================================

// Membaca record terdepan.
// Event diprioritaskan dibanding telemetry.

bool offlineStoragePeek(OfflineRecord& record);

// ============================================================
// ACKNOWLEDGED RECORD REMOVAL
// ============================================================

// Menghapus record berdasarkan ID yang telah dikonfirmasi backend.
// Jangan dipanggil hanya karena MQTT publish berhasil.

bool offlineStorageRemove(const char* recordId);

// ============================================================
// QUEUE STATISTICS
// ============================================================

size_t offlineStorageTelemetryCount();

size_t offlineStorageEventCount();

#endif
