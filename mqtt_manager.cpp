
#include "mqtt_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <stdio.h>

#include "offline_storage.h"

// ============================================================
// LANDIA FARM - MQTT MANAGER
//
// Kontrol irigasi tidak ditempatkan di file ini.
// MQTT hanya menangani komunikasi dan antrean data.
// ============================================================

const char* WIFI_SSID = "AYU";
const char* WIFI_PASSWORD = "ayu241283";

const char* MQTT_SERVER = "192.168.0.234";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_CLIENT_ID = "landia-esp32-01";

// Topic yang sudah digunakan Landia Farm.
const char* TOPIC_DHT =
  "landia/sensor/dht";

const char* TOPIC_SOIL =
  "landia/sensor/soil";

const char* TOPIC_WATER_LEVEL =
  "landia/sensor/water-level";

const char* TOPIC_RAIN =
  "landia/sensor/rain";

const char* TOPIC_FLOW =
  "landia/sensor/flow";

const char* TOPIC_IRRIGATION =
  "landia/actuator/irrigation";

const char* TOPIC_IRRIGATION_EVENT =
  "landia/irrigation/event";

// ============================================================
// MQTT OBJECTS
// ============================================================

namespace {

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const unsigned long WIFI_RECONNECT_INTERVAL = 10000UL;
const unsigned long MQTT_RECONNECT_INTERVAL = 10000UL;

const unsigned long OFFLINE_TELEMETRY_INTERVAL =
  300000UL;  // 5 menit

const unsigned long BACKLOG_SEND_INTERVAL =
  500UL;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastOfflineTelemetryStore = 0;
unsigned long lastBacklogSend = 0;

bool storageReady = false;

// ------------------------------------------------------------
// DECLARATIONS
// ------------------------------------------------------------

void mqttEnsureWiFi();
void mqttConnect();

bool mqttPublishJson(
  const char* topic,
  const char* payload,
  bool isEvent
);

void queueTelemetrySet(
  const DHTData& dht,
  const SoilData& soil,
  const RainData& rain,
  const FlowData& flow,
  const WaterLevelData& waterLevel
);

bool publishOneBacklogRecord();

// ============================================================
// GENERIC PUBLISH
// ============================================================

bool mqttPublishJson(
  const char* topic,
  const char* payload,
  bool isEvent
) {
  if (topic == nullptr || payload == nullptr) {
    return false;
  }

  if (mqttClient.connected()) {
    if (mqttClient.publish(topic, payload, false)) {
      return true;
    }

    Serial.printf(
      "[MQTT] Publish gagal: %s\n",
      topic
    );
  }

  if (!storageReady) {
    Serial.println(
      "[OFFLINE] Penyimpanan tidak tersedia; pesan gagal diantrikan."
    );

    return false;
  }

  bool queued = offlineStorageEnqueue(
    topic,
    payload,
    isEvent
  );

  if (!queued) {
    Serial.printf(
      "[OFFLINE] Pesan gagal disimpan: %s\n",
      topic
    );
  }

  return queued;
}

// ============================================================
// STORE TELEMETRY SET
// ============================================================


void queueTelemetrySet(
  const DHTData& dht,
  const SoilData& soil,
  const RainData& rain,
  const FlowData& flow,
  const WaterLevelData& waterLevel
) {
  char payload[256];

  // 1. DHT11
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-dht-01\","
    "\"temperature\":%.2f,"
    "\"humidity\":%.2f}",
    dht.temperature,
    dht.humidity
  );
  offlineStorageEnqueue(TOPIC_DHT, payload, false);

  // 2. Sensor kelembapan tanah
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-soil-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    soil.moisturePercent
  );
  offlineStorageEnqueue(TOPIC_SOIL, payload, false);

  // 3. Sensor hujan
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-rain-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    rain.rainPercent
  );
  offlineStorageEnqueue(TOPIC_RAIN, payload, false);

  // 4. Flow sensor
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-flow-01\","
    "\"value\":%.3f,\"unit\":\"L/min\"}",
    flow.flowRateLMin
  );
  offlineStorageEnqueue(TOPIC_FLOW, payload, false);

  // 5. Sensor level air
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-water-level-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    waterLevel.waterLevelPercent
  );
  offlineStorageEnqueue(TOPIC_WATER_LEVEL, payload, false);
}

// ============================================================
// WIFI RECONNECT
// ============================================================

void mqttEnsureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiAttempt <
      WIFI_RECONNECT_INTERVAL) {
    return;
  }

  lastWiFiAttempt = now;

  Serial.println(
    "[MQTT] Wi-Fi terputus; mencoba reconnect."
  );

  // Jangan melakukan loop tunggu koneksi di sini.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================
// MQTT RECONNECT
// ============================================================

void mqttConnect() {
  if (WiFi.status() != WL_CONNECTED ||
      mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMQTTAttempt <
      MQTT_RECONNECT_INTERVAL) {
    return;
  }

  lastMQTTAttempt = now;

  Serial.println(
    "[MQTT] Mencoba koneksi ke broker."
  );

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println(
      "[MQTT] Berhasil terhubung."
    );

    mqttClient.publish(
      TOPIC_IRRIGATION_EVENT,
      "{\"event\":\"device_online\"}",
      false
    );
  } else {
    Serial.printf(
      "[MQTT] Koneksi gagal. State=%d\n",
      mqttClient.state()
    );
  }
}

// ============================================================
// SEND BACKLOG
// Event diprioritaskan oleh offlineStoragePeek().
// ============================================================

bool publishOneBacklogRecord() {
  if (!mqttClient.connected() || !storageReady) {
    return false;
  }

  OfflineRecord record;

  if (!offlineStoragePeek(record)) {
    return false;
  }

  if (!mqttClient.publish(
        record.topic.c_str(),
        record.payload.c_str(),
        false)) {
    Serial.printf(
      "[MQTT] Pengiriman backlog gagal: %s\n",
      record.topic.c_str()
    );

    return false;
  }

  // PENTING:
  // Publish QoS 0 bukan ACK dari database server.
  // Penghapusan ini belum memberikan jaminan end-to-end.
  if (!offlineStorageRemoveFirst(record.isEvent)) {
    Serial.println(
      "[OFFLINE] Gagal menghapus record setelah publish."
    );

    return false;
  }

  Serial.printf(
    "[OFFLINE] Record dikirim: %s\n",
    record.topic.c_str()
  );

  lastBacklogSend = millis();

  return true;
}

} // namespace

// ============================================================
// INIT
// ============================================================

void mqttInit() {
  Serial.println();
  Serial.println("========== MQTT INIT ==========");

  Serial.println(
    "[DEBUG] Memanggil offlineStorageBegin()..."
  );

  storageReady = offlineStorageBegin();

  Serial.printf(
    "[DEBUG] storageReady = %s\n",
    storageReady ? "true" : "false"
  );

  Serial.printf(
    "[DEBUG] offlineStorageIsReady() = %s\n",
    offlineStorageIsReady() ? "true" : "false"
  );

  if (!storageReady) {
    Serial.println(
      "[OFFLINE] PERINGATAN: buffer offline tidak tersedia."
    );
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqttClient.setSocketTimeout(2);
  mqttClient.setKeepAlive(15);
  mqttClient.setBufferSize(768);

  Serial.println(
    "[MQTT] Wi-Fi connection started."
  );

  Serial.println(
    "[MQTT] Manager ready."
  );
}

// ============================================================
// STATUS
// ============================================================

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool mqttIsConnected() {
  return mqttClient.connected();
}

// ============================================================
// UPDATE
// ============================================================

void mqttUpdate() {
  if (!wifiIsConnected()) {
    mqttEnsureWiFi();
    return;
  }

  if (!mqttClient.connected()) {
    mqttConnect();
    return;
  }

  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastBacklogSend >=
      BACKLOG_SEND_INTERVAL) {
    publishOneBacklogRecord();
  }
}

// ============================================================
// TELEMETRY
// ============================================================


void mqttPublishTelemetry(
  const DHTData& dht,
  const SoilData& soil,
  const RainData& rain,
  const FlowData& flow,
  const WaterLevelData& waterLevel,
  const RTCData& rtc
) {
  // Timestamp RTC belum dimasukkan ke payload.
  (void)rtc;

  // Jika MQTT terputus, simpan seluruh telemetry ke antrean.
  if (!mqttClient.connected()) {
    if (!storageReady) {
      Serial.println(
        "[OFFLINE] Telemetry tidak disimpan: "
        "penyimpanan offline tidak tersedia."
      );
      return;
    }

    unsigned long now = millis();

    if (
      lastOfflineTelemetryStore != 0 &&
      now - lastOfflineTelemetryStore <
        OFFLINE_TELEMETRY_INTERVAL
    ) {
      return;
    }

    lastOfflineTelemetryStore = now;

    queueTelemetrySet(
      dht,
      soil,
      rain,
      flow,
      waterLevel
    );

    Serial.printf(
      "[OFFLINE] Events=%u, telemetry=%u\n",
      (unsigned int)offlineStorageEventCount(),
      (unsigned int)offlineStorageTelemetryCount()
    );

    return;
  }

  char payload[256];

  // 1. DHT11
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-dht-01\","
    "\"temperature\":%.2f,"
    "\"humidity\":%.2f}",
    dht.temperature,
    dht.humidity
  );
  mqttPublishJson(TOPIC_DHT, payload, false);

  // 2. Sensor kelembapan tanah
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-soil-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    soil.moisturePercent
  );
  mqttPublishJson(TOPIC_SOIL, payload, false);

  // 3. Sensor hujan
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-rain-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    rain.rainPercent
  );
  mqttPublishJson(TOPIC_RAIN, payload, false);

  // 4. Flow sensor
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-flow-01\","
    "\"value\":%.3f,\"unit\":\"L/min\"}",
    flow.flowRateLMin
  );
  mqttPublishJson(TOPIC_FLOW, payload, false);

  // 5. Sensor level air
  snprintf(
    payload,
    sizeof(payload),
    "{\"sensorId\":\"landia-water-level-01\","
    "\"value\":%.2f,\"unit\":\"%%\"}",
    waterLevel.waterLevelPercent
  );
  mqttPublishJson(TOPIC_WATER_LEVEL, payload, false);
}


// ============================================================
// IRRIGATION EVENT
// ============================================================

void mqttPublishIrrigationEvent(
  const char* event,
  const char* state,
  const RTCData& rtc
) {
  (void)rtc;

  if (event == nullptr || state == nullptr) {
    return;
  }

  char payload[256];

  String timestamp = rtcGetTimestamp();

  snprintf(
    payload,
    sizeof(payload),
    "{\"event\":\"%s\",\"state\":\"%s\","
    "\"timestamp\":\"%s\"}",
    event,
    state,
    timestamp.c_str()
  );

  mqttPublishJson(
    TOPIC_IRRIGATION_EVENT,
    payload,
    true
  );
}
