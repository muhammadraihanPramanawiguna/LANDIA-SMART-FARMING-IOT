
#ifndef OFFLINE_STORAGE_H
#define OFFLINE_STORAGE_H

#include <Arduino.h>

struct OfflineRecord {
  String topic;
  String payload;
  bool isEvent;
};

bool offlineStorageBegin();
bool offlineStorageIsReady();

bool offlineStorageEnqueue(
  const char* topic,
  const char* payload,
  bool isEvent
);

bool offlineStoragePeek(OfflineRecord& record);
bool offlineStorageRemoveFirst(bool isEvent);

size_t offlineStorageTelemetryCount();
size_t offlineStorageEventCount();

#endif
