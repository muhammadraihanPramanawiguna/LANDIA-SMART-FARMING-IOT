
#include "offline_storage.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

// ============================================================
// LANDIA FARM - OFFLINE STORAGE
// ============================================================

namespace {

const char* EVENT_FILE = "/offline_events.log";
const char* TELEMETRY_FILE = "/offline_telemetry.log";

const char* EVENT_TEMP = "/offline_events.tmp";
const char* TELEMETRY_TEMP = "/offline_telemetry.tmp";

const char* EVENT_BACKUP = "/offline_events.bak";
const char* TELEMETRY_BACKUP = "/offline_telemetry.bak";

const size_t MAX_STORAGE_PERCENT = 70;
const size_t EVENT_RESERVE_PERCENT = 15;

bool storageReady = false;

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
// FILE UTILITIES
// ------------------------------------------------------------

size_t countLines(const char* path) {
  File file = LittleFS.open(path, "r");

  if (!file) {
    return 0;
  }

  size_t count = 0;

  while (file.available()) {
    if (file.read() == '\n') {
      ++count;
    }
  }

  file.close();
  return count;
}

bool validateQueueLine(
  const String& line,
  bool expectedEvent,
  OfflineRecord& record
) {
  int firstTab = line.indexOf('\t');

  if (firstTab != 1) {
    return false;
  }

  int secondTab = line.indexOf('\t', firstTab + 1);

  if (secondTab < 0) {
    return false;
  }

  char type = line[0];

  if (type != 'E' && type != 'T') {
    return false;
  }

  record.isEvent = (type == 'E');

  if (record.isEvent != expectedEvent) {
    return false;
  }

  record.topic = line.substring(firstTab + 1, secondTab);
  record.payload = line.substring(secondTab + 1);

  return record.topic.length() > 0;
}

bool readFirstRecord(
  bool isEvent,
  OfflineRecord& record
) {
  File file = LittleFS.open(queuePath(isEvent), "r");

  if (!file) {
    return false;
  }

  String line = file.readStringUntil('\n');
  file.close();

  if (line.length() == 0) {
    return false;
  }

  if (!validateQueueLine(line, isEvent, record)) {
    Serial.println(
      "[OFFLINE] ERROR: format record antrean tidak valid."
    );

    return false;
  }

  return true;
}

// ------------------------------------------------------------
// RECOVERY
// ------------------------------------------------------------

bool recoverQueueFiles(bool isEvent) {
  const char* path = queuePath(isEvent);
  const char* temp = tempPath(isEvent);
  const char* backup = backupPath(isEvent);

  // Jika file utama hilang, pulihkan backup.
  if (!LittleFS.exists(path) &&
      LittleFS.exists(backup)) {
    if (!LittleFS.rename(backup, path)) {
      Serial.printf(
        "[OFFLINE] ERROR: gagal memulihkan %s\n",
        path
      );

      return false;
    }
  }

  // File utama yang tersedia menjadi sumber kebenaran.
  if (LittleFS.exists(path) &&
      LittleFS.exists(backup)) {
    if (!LittleFS.remove(backup)) {
      Serial.printf(
        "[OFFLINE] WARNING: backup belum terhapus: %s\n",
        backup
      );
    }
  }

  // Temporary tidak pernah dipakai sebagai antrean utama.
  if (LittleFS.exists(temp)) {
    if (!LittleFS.remove(temp)) {
      Serial.printf(
        "[OFFLINE] WARNING: temp belum terhapus: %s\n",
        temp
      );
    }
  }

  return true;
}

// ------------------------------------------------------------
// RECORD FORMAT
// E<TAB>topic<TAB>payload<NEWLINE>
// T<TAB>topic<TAB>payload<NEWLINE>
// ------------------------------------------------------------

String makeRecord(
  const char* topic,
  const char* payload,
  bool isEvent
) {
  String line;

  line.reserve(strlen(topic) + strlen(payload) + 5);

  line = isEvent ? "E\t" : "T\t";
  line += topic;
  line += '\t';
  line += payload;
  line += '\n';

  return line;
}

// ------------------------------------------------------------
// REMOVE FIRST RECORD
// Menyalin antrean kecuali rekaman pertama.
// ------------------------------------------------------------

bool copyRemainingRecords(
  File& source,
  File& destination
) {
  bool skippedFirst = false;

  while (source.available()) {
    String line = source.readStringUntil('\n');

    if (!skippedFirst) {
      skippedFirst = true;
      continue;
    }

    if (line.length() == 0) {
      continue;
    }

    line += '\n';

    size_t expected = line.length();
    size_t written = destination.print(line);

    if (written != expected) {
      return false;
    }
  }

  return skippedFirst;
}

} // namespace

// ============================================================
// PUBLIC API
// ============================================================

bool offlineStorageIsReady() {
  return storageReady;
}

bool offlineStorageBegin() {
  storageReady = false;

  Serial.println();
  Serial.println("========== OFFLINE STORAGE INIT ==========");

  // Jangan format otomatis jika mount gagal.
  if (!LittleFS.begin(false)) {
    Serial.println(
      "[OFFLINE] ERROR: LittleFS gagal di-mount."
    );

    Serial.println(
      "[OFFLINE] Periksa partisi dan kondisi filesystem."
    );

    return false;
  }

  Serial.println(
    "[OFFLINE] LittleFS berhasil di-mount."
  );

  if (!recoverQueueFiles(true)) {
    LittleFS.end();

    Serial.println(
      "[OFFLINE] ERROR: recovery antrean event gagal."
    );

    return false;
  }

  if (!recoverQueueFiles(false)) {
    LittleFS.end();

    Serial.println(
      "[OFFLINE] ERROR: recovery telemetry gagal."
    );

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
    "[OFFLINE] Events: %u\n",
    (unsigned int)offlineStorageEventCount()
  );

  Serial.printf(
    "[OFFLINE] Telemetry: %u\n",
    (unsigned int)offlineStorageTelemetryCount()
  );

  Serial.println("[OFFLINE] LittleFS READY.");

  return true;
}

bool offlineStorageEnqueue(
  const char* topic,
  const char* payload,
  bool isEvent
) {
  if (!storageReady ||
      topic == nullptr ||
      payload == nullptr ||
      topic[0] == '\0') {
    return false;
  }

  // TAB/newline akan merusak format antrean.
  if (strchr(topic, '\t') ||
      strchr(topic, '\n') ||
      strchr(topic, '\r') ||
      strchr(payload, '\t') ||
      strchr(payload, '\n') ||
      strchr(payload, '\r')) {
    Serial.println(
      "[OFFLINE] ERROR: karakter pemisah tidak valid."
    );

    return false;
  }

  String line = makeRecord(topic, payload, isEvent);

  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();

  if (total == 0 || line.length() == 0) {
    return false;
  }

  const size_t overallLimit =
    total * MAX_STORAGE_PERCENT / 100;

  const size_t eventReserve =
    total * EVENT_RESERVE_PERCENT / 100;

  const size_t telemetryLimit =
    overallLimit - eventReserve;

  File existing = LittleFS.open(queuePath(isEvent), "r");

  size_t queueBytes = existing ? existing.size() : 0;

  if (existing) {
    existing.close();
  }

  if (used + line.length() > overallLimit) {
    Serial.println(
      "[OFFLINE] Batas penggunaan filesystem tercapai."
    );

    return false;
  }

  if (!isEvent &&
      used + line.length() > telemetryLimit) {
    Serial.println(
      "[OFFLINE] Batas antrean telemetry tercapai."
    );

    return false;
  }

  if (isEvent &&
      queueBytes + line.length() > eventReserve) {
    Serial.println(
      "[OFFLINE] Batas antrean event tercapai."
    );

    return false;
  }

  File file = LittleFS.open(queuePath(isEvent), "a");

  if (!file) {
    Serial.println(
      "[OFFLINE] ERROR: file antrean gagal dibuka."
    );

    return false;
  }

  const size_t expected = line.length();
  const size_t written = file.print(line);

  file.flush();
  file.close();

  if (written != expected) {
    Serial.println(
      "[OFFLINE] ERROR: penulisan record tidak lengkap."
    );

    return false;
  }

  return true;
}

bool offlineStoragePeek(OfflineRecord& record) {
  if (!storageReady) {
    return false;
  }

  // Prioritaskan event sebelum telemetry.
  if (readFirstRecord(true, record)) {
    return true;
  }

  return readFirstRecord(false, record);
}

bool offlineStorageRemoveFirst(bool isEvent) {
  if (!storageReady) {
    return false;
  }

  const char* path = queuePath(isEvent);
  const char* temp = tempPath(isEvent);
  const char* backup = backupPath(isEvent);

  File source = LittleFS.open(path, "r");

  if (!source) {
    return false;
  }

  File replacement = LittleFS.open(temp, "w");

  if (!replacement) {
    source.close();

    Serial.println(
      "[OFFLINE] ERROR: file temporary gagal dibuka."
    );

    return false;
  }

  bool copied = copyRemainingRecords(source, replacement);

  replacement.flush();
  source.close();
  replacement.close();

  if (!copied) {
    LittleFS.remove(temp);

    Serial.println(
      "[OFFLINE] ERROR: penyalinan antrean gagal."
    );

    return false;
  }

  if (LittleFS.exists(backup) &&
      !LittleFS.remove(backup)) {
    LittleFS.remove(temp);

    Serial.println(
      "[OFFLINE] ERROR: backup lama gagal dihapus."
    );

    return false;
  }

  // Simpan file utama sebelum menggantinya.
  if (!LittleFS.rename(path, backup)) {
    LittleFS.remove(temp);

    Serial.println(
      "[OFFLINE] ERROR: gagal membuat backup antrean."
    );

    return false;
  }

  if (!LittleFS.rename(temp, path)) {
    bool restored = LittleFS.rename(backup, path);

    Serial.println(
      restored
        ? "[OFFLINE] File lama dipulihkan."
        : "[OFFLINE] KRITIS: pemulihan file gagal."
    );

    return false;
  }

  if (LittleFS.exists(backup) &&
      !LittleFS.remove(backup)) {
    Serial.println(
      "[OFFLINE] WARNING: backup masih tersisa."
    );
  }

  return true;
}

size_t offlineStorageTelemetryCount() {
  if (!storageReady) {
    return 0;
  }

  return countLines(TELEMETRY_FILE);
}

size_t offlineStorageEventCount() {
  if (!storageReady) {
    return 0;
  }

  return countLines(EVENT_FILE);
}
