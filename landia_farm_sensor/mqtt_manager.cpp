#include "mqtt_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <espMqttClient.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "offline_storage.h"
#include "secrets.h"

// ============================================================
// LANDIA FARM - THINGSBOARD MQTT MANAGER
//
// MQTT endpoint: 194.233.73.154:1883
// Topic        : esp/telemetry (terbukti pada server aktual)
// Authentication: Device Access Token sebagai MQTT username
// QoS          : 1
//
// Telemetry disimpan lebih dahulu ke LittleFS. Record baru dihapus
// setelah callback onPublish menerima PUBACK dari broker.
//
// Catatan: PUBACK membuktikan broker menerima publish QoS 1,
// bukan bukti transaksi database ThingsBoard. Verifikasi Latest
// Telemetry tetap dilakukan di dashboard.
// ============================================================

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#error "WIFI_SSID dan WIFI_PASSWORD harus didefinisikan di secrets.h"
#endif

#if !defined(THINGSBOARD_DEVICE_TOKEN)
#error "THINGSBOARD_DEVICE_TOKEN harus didefinisikan di secrets.h"
#endif

namespace {

constexpr char MQTT_CLIENT_ID[] = "landia-esp32-01";
constexpr char TELEMETRY_TOPIC[] = THINGSBOARD_MQTT_TOPIC;
constexpr size_t TELEMETRY_PAYLOAD_SIZE = 480;

espMqttClient mqttClient;

bool storageReady = false;
bool wifiStarted = false;
bool wifiWasConnected = false;
bool mqttConnectPending = false;

volatile bool mqttConnectedEvent = false;
volatile bool mqttDisconnectedEvent = false;
volatile uint16_t lastPublishAckPacketId = 0;

unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastBacklogSend = 0;
unsigned long lastConnectionLog = 0;

String inFlightRecordId;
uint16_t inFlightPacketId = 0;

// ------------------------------------------------------------
// MQTT EVENT CALLBACKS
// Callback library berjalan pada task internal espMqttClient.
// Operasi LittleFS dilakukan di mqttUpdate(), bukan di callback.
// ------------------------------------------------------------

void onMqttConnect(bool sessionPresent) {
    (void)sessionPresent;
    mqttConnectedEvent = true;
    mqttDisconnectedEvent = false;
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
    (void)reason;
    mqttDisconnectedEvent = true;
    mqttConnectedEvent = false;
}

void onMqttPublish(uint16_t packetId) {
    // Callback ini menandakan PUBACK untuk publish QoS 1.
    lastPublishAckPacketId = packetId;
}

// ------------------------------------------------------------
// WIFI
// ------------------------------------------------------------

void ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            Serial.printf("[WIFI] Connected. IP: %s\n",
                          WiFi.localIP().toString().c_str());
        }
        return;
    }

    wifiWasConnected = false;
    mqttConnectPending = false;

    const unsigned long now = millis();
    if (!wifiStarted ||
        now - lastWifiAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
        wifiStarted = true;
        lastWifiAttempt = now;

        Serial.println("[WIFI] Connecting/reconnecting to hotspot...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

// ------------------------------------------------------------
// PAYLOAD NORMALIZATION
//
// Payload baru sudah berupa telemetry ThingsBoard.
// Record LittleFS lama dari versi backend sebelumnya dinormalisasi
// agar hanya key telemetry yang dikenal yang dikirim ke esp/telemetry.
// Record yang tidak dikenali tidak dihapus otomatis.
// ------------------------------------------------------------

bool copyNumeric(JsonDocument& destination,
                 const char* destinationKey,
                 JsonDocument& source,
                 const char* sourceKey) {
    JsonVariantConst value = source[sourceKey];
    if (value.isNull() || !value.is<float>()) return false;
    const float number = value.as<float>();
    if (!isfinite(number)) return false;
    destination[destinationKey] = number;
    return true;
}

bool normalizeTelemetryPayload(const OfflineRecord& record,
                               char* output,
                               size_t outputSize) {
    if (output == nullptr || outputSize == 0) return false;

    JsonDocument source;
    DeserializationError error = deserializeJson(source, record.payload);
    if (error) {
        Serial.printf("[MQTT] Legacy payload JSON invalid: %s\n",
                      error.c_str());
        return false;
    }

    JsonDocument telemetry;
    bool hasValue = false;

    hasValue |= copyNumeric(telemetry, "temperature", source, "temperature");
    hasValue |= copyNumeric(telemetry, "humidity", source, "humidity");
    hasValue |= copyNumeric(telemetry, "soil_moisture", source, "soil_moisture");
    hasValue |= copyNumeric(telemetry, "rain_percent", source, "rain_percent");
    hasValue |= copyNumeric(telemetry, "flow_rate", source, "flow_rate");
    hasValue |= copyNumeric(telemetry, "total_water_liters", source, "total_water_liters");
    hasValue |= copyNumeric(telemetry, "water_consumption_m3", source, "water_consumption_m3");
    hasValue |= copyNumeric(telemetry, "water_cost_rupiah", source, "water_cost_rupiah");
    hasValue |= copyNumeric(telemetry, "water_tariff_rupiah_per_m3", source, "water_tariff_rupiah_per_m3");
    hasValue |= copyNumeric(telemetry, "water_level_distance_cm", source, "water_level_distance_cm");
    hasValue |= copyNumeric(telemetry, "water_level_percent", source, "water_level_percent");

    // Record dari firmware backend lama memakai sensorId + value.
    const char* sensorId = source["sensorId"] | "";
    if (strcmp(sensorId, "landia-soil-01") == 0) {
        hasValue |= copyNumeric(telemetry, "soil_moisture", source, "value");
    } else if (strcmp(sensorId, "landia-rain-01") == 0) {
        hasValue |= copyNumeric(telemetry, "rain_percent", source, "value");
    } else if (strcmp(sensorId, "landia-flow-01") == 0) {
        hasValue |= copyNumeric(telemetry, "flow_rate", source, "value");
    } else if (strcmp(sensorId, "landia-water-level-01") == 0) {
        hasValue |= copyNumeric(telemetry, "water_level_distance_cm", source, "value");
    }

    // is_raining disimpan sebagai boolean ThingsBoard.
    if (!source["is_raining"].isNull()) {
        telemetry["is_raining"] = source["is_raining"].as<bool>();
        hasValue = true;
    }

    if (!hasValue) {
        Serial.printf("[MQTT] Record %s tidak memiliki key telemetry yang dikenali. "
                      "Record dipertahankan.\n",
                      record.recordId.c_str());
        return false;
    }

    const size_t written = serializeJson(telemetry, output, outputSize);
    if (written == 0 || written >= outputSize) return false;
    output[written] = '\0';
    return true;
}

// ------------------------------------------------------------
// ENQUEUE ONE COMBINED SENSOR SNAPSHOT
// ------------------------------------------------------------

bool enqueueTelemetrySnapshot(const DHTData& dht,
                              const SoilData& soil,
                              const RainData& rain,
                              const FlowData& flow,
                              const WaterLevelData& level) {
    JsonDocument telemetry;
    bool hasValue = false;

    if (dht.valid &&
        isfinite(dht.temperature) &&
        isfinite(dht.humidity)) {
        telemetry["temperature"] = dht.temperature;
        telemetry["humidity"] = dht.humidity;
        hasValue = true;
    }

    if (soil.valid && isfinite(soil.moisturePercent)) {
        telemetry["soil_moisture"] = soil.moisturePercent;
        hasValue = true;
    }

    if (rain.valid && isfinite(rain.rainPercent)) {
        telemetry["rain_percent"] = rain.rainPercent;
        telemetry["is_raining"] = rain.raining;
        hasValue = true;
    }

    if (flow.valid &&
        isfinite(flow.flowRateLMin) &&
        isfinite(flow.totalLitres)) {
        telemetry["flow_rate"] = flow.flowRateLMin;
        telemetry["total_water_liters"] = flow.totalLitres;

        // Volume dan biaya dihitung lokal oleh flow_sensor.cpp.
        // Satuan biaya adalah Rupiah; tarif dikonfigurasi dalam Rp/m3.
        if (isfinite(flow.totalCubicMeter) && flow.totalCubicMeter >= 0.0f) {
            telemetry["water_consumption_m3"] = flow.totalCubicMeter;
        }

        if (isfinite(flow.totalCost) && flow.totalCost >= 0.0f) {
            telemetry["water_cost_rupiah"] = flow.totalCost;
        }

        if (isfinite(static_cast<float>(WATER_TARIFF_PER_M3)) &&
            WATER_TARIFF_PER_M3 >= 0.0f) {
            telemetry["water_tariff_rupiah_per_m3"] = WATER_TARIFF_PER_M3;
        }

        hasValue = true;
    }

// WATER LEVEL - JSN-SR04M
Serial.printf(
    "[MQTT][WATER] valid=%d | distance=%.2f cm | level=%.2f%%\n",
    level.valid,
    level.distanceCm,
    level.waterLevelPercent
);

if (level.valid &&
    isfinite(level.distanceCm) &&
    isfinite(level.waterLevelPercent) &&
    level.distanceCm >= 0.0f &&
    level.waterLevelPercent >= 0.0f &&
    level.waterLevelPercent <= 100.0f) {

    telemetry["water_level_distance_cm"] = level.distanceCm;
    telemetry["water_level_percent"] = level.waterLevelPercent;

    hasValue = true;

    Serial.println("[MQTT][WATER] Water level masuk payload.");
} else {
    Serial.println("[MQTT][WATER] Data tidak valid, tidak dimasukkan.");
}

    if (!hasValue) {
        Serial.println("[MQTT] No valid sensor values; snapshot not queued.");
        return false;
    }

    char payload[TELEMETRY_PAYLOAD_SIZE];
    const size_t length = serializeJson(telemetry, payload, sizeof(payload));
    if (length == 0 || length >= sizeof(payload)) {
        Serial.println("[MQTT] Telemetry payload exceeds buffer; not queued.");
        return false;
    }
    payload[length] = '\0';

    Serial.printf("[MQTT][TELEMETRY] Snapshot payload (%u bytes): %s\\n",
                  static_cast<unsigned int>(length), payload);

    char recordId[41];
    if (!offlineStorageGenerateRecordId(recordId, sizeof(recordId))) {
        Serial.println("[OFFLINE] Failed to generate telemetry record ID.");
        return false;
    }

    if (!offlineStorageEnqueue(recordId,
                               TELEMETRY_TOPIC,
                               payload,
                               false)) {
        Serial.println("[OFFLINE] Failed to enqueue telemetry snapshot.");
        return false;
    }

    Serial.printf("[OFFLINE] Telemetry queued: %s (%u bytes)\n",
                  recordId, (unsigned int)length);
    return true;
}

// ------------------------------------------------------------
// PROCESS BROKER PUBACK
// ------------------------------------------------------------

void processPublishAck() {
    const uint16_t ackId = lastPublishAckPacketId;

    if (ackId == 0 ||
        inFlightPacketId == 0 ||
        ackId != inFlightPacketId ||
        inFlightRecordId.length() == 0) {
        return;
    }

    if (offlineStorageRemove(inFlightRecordId.c_str())) {
        Serial.printf("[MQTT] PUBACK received. Queue record removed: %s\n",
                      inFlightRecordId.c_str());
        inFlightRecordId = "";
        inFlightPacketId = 0;
        lastPublishAckPacketId = 0;
    } else {
        Serial.printf("[OFFLINE] PUBACK received, but failed to remove %s. "
                      "Will retry removal.\n",
                      inFlightRecordId.c_str());
    }
}

// ------------------------------------------------------------
// SEND ONE QUEUED RECORD
// ------------------------------------------------------------

void publishOneQueuedTelemetry() {
    if (!mqttClient.connected() ||
        !storageReady ||
        inFlightPacketId != 0) {
        return;
    }

    const unsigned long now = millis();
    if (now - lastBacklogSend < MQTT_BACKLOG_SEND_INTERVAL_MS) return;

    OfflineRecord record;
    if (!offlineStoragePeekTelemetry(record)) return;

    if (record.isEvent) {
        Serial.println("[MQTT] Event record found in telemetry queue; "
                       "sending stopped for safety.");
        return;
    }

    char normalizedPayload[TELEMETRY_PAYLOAD_SIZE];
    if (!normalizeTelemetryPayload(record,
                                   normalizedPayload,
                                   sizeof(normalizedPayload))) {
        // Do not delete unknown or malformed records automatically.
        return;
    }

    const uint16_t packetId = mqttClient.publish(
        TELEMETRY_TOPIC,
        THINGSBOARD_MQTT_QOS,
        false,
        normalizedPayload
    );

    if (packetId == 0) {
        Serial.println("[MQTT] Publish enqueue failed; LittleFS record retained.");
        lastBacklogSend = now;
        return;
    }

    inFlightRecordId = record.recordId;
    inFlightPacketId = packetId;
    lastBacklogSend = now;

    Serial.printf("[MQTT] PUBLISH queued: packetId=%u | record=%s | topic=%s | QoS=%d\n",
                  packetId,
                  record.recordId.c_str(),
                  TELEMETRY_TOPIC,
                  THINGSBOARD_MQTT_QOS);
    Serial.printf("[MQTT] Payload sent: %s\\n", normalizedPayload);
}

// ------------------------------------------------------------
// EVENT QUEUE
// ------------------------------------------------------------

void enqueueIrrigationEvent(const char* event,
                            const char* state,
                            const RTCData& rtc) {
    if (!storageReady || event == nullptr || state == nullptr ||
        event[0] == '\0' || state[0] == '\0') {
        return;
    }

    JsonDocument document;
    document["event"] = event;
    document["state"] = state;

    if (rtc.valid) {
        document["year"] = rtc.year;
        document["month"] = rtc.month;
        document["day"] = rtc.day;
        document["hour"] = rtc.hour;
        document["minute"] = rtc.minute;
        document["second"] = rtc.second;
    }

    char payload[256];
    const size_t length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || length >= sizeof(payload)) {
        Serial.println("[EVENT] Failed to serialize irrigation event.");
        return;
    }
    payload[length] = '\0';

    char recordId[41];
    if (!offlineStorageGenerateRecordId(recordId, sizeof(recordId))) return;

    // Event sengaja dipertahankan lokal karena schema/consumer event
    // ThingsBoard belum dikonfirmasi pada server Customer ini.
    if (!offlineStorageEnqueue(recordId,
                               "landia/irrigation/event",
                               payload,
                               true)) {
        Serial.println("[EVENT] Failed to store irrigation event.");
        return;
    }

    Serial.printf("[EVENT] Stored locally: %s | %s | %s\n",
                  recordId, event, state);
}

} // namespace

// ============================================================
// PUBLIC API
// ============================================================

void mqttInit() {
    Serial.println();
    Serial.println("========== LANDIA THINGSBOARD MQTT INIT ==========");

    storageReady = offlineStorageBegin();
    if (!storageReady) {
        Serial.println("[OFFLINE] LittleFS unavailable; telemetry queue disabled.");
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    mqttClient.setServer(THINGSBOARD_MQTT_HOST, THINGSBOARD_MQTT_PORT);
    mqttClient.setClientId(MQTT_CLIENT_ID);
    mqttClient.setCredentials(THINGSBOARD_DEVICE_TOKEN, nullptr);
    mqttClient.setKeepAlive(30);
    mqttClient.setTimeout(10);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onPublish(onMqttPublish);

    // Wi-Fi connection starts asynchronously; setup() does not wait.
    ensureWiFi();

    Serial.printf("[MQTT] Initialized: %s:%u | topic=%s | QoS=%d\n",
                  THINGSBOARD_MQTT_HOST,
                  THINGSBOARD_MQTT_PORT,
                  TELEMETRY_TOPIC,
                  THINGSBOARD_MQTT_QOS);
}

void mqttUpdate() {
    // Consume acknowledgement first so a received PUBACK is not lost
    // if the disconnect callback is also pending.
    processPublishAck();

    if (mqttDisconnectedEvent) {
        mqttDisconnectedEvent = false;
        mqttConnectPending = false;

        // Broker did not acknowledge the in-flight record.
        // Keep it in LittleFS and allow retransmission after reconnect.
        if (inFlightPacketId != 0) {
            Serial.printf("[MQTT] Disconnected before PUBACK. "
                          "Retaining record: %s\n",
                          inFlightRecordId.c_str());
            inFlightRecordId = "";
            inFlightPacketId = 0;
            lastPublishAckPacketId = 0;
        }
    }

    if (mqttConnectedEvent) {
        mqttConnectedEvent = false;
        mqttConnectPending = false;
        Serial.println("[MQTT] Connected to ThingsBoard broker.");
    }

    ensureWiFi();

    if (WiFi.status() != WL_CONNECTED) {
        mqttConnectPending = false;
        return;
    }

    if (!mqttClient.connected()) {
        const unsigned long now = millis();
        if (!mqttConnectPending &&
            now - lastMqttAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            lastMqttAttempt = now;
            mqttConnectPending = mqttClient.connect();
            if (mqttConnectPending) {
                Serial.println("[MQTT] Connection attempt started.");
            } else {
                Serial.println("[MQTT] Could not start connection attempt.");
            }
        }
        return;
    }

    mqttConnectPending = false;
    processPublishAck();
    publishOneQueuedTelemetry();
}

bool mqttIsConnected() {
    return WiFi.status() == WL_CONNECTED && mqttClient.connected();
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void mqttPublishTelemetry(const DHTData& dht,
                          const SoilData& soil,
                          const RainData& rain,
                          const FlowData& flow,
                          const WaterLevelData& waterLevel,
                          const RTCData& rtc) {
    (void)rtc;

    if (!storageReady) {
        Serial.println("[OFFLINE] Cannot queue telemetry: LittleFS unavailable.");
        return;
    }

    enqueueTelemetrySnapshot(dht, soil, rain, flow, waterLevel);
}

void mqttPublishIrrigationEvent(const char* event,
                                const char* state,
                                const RTCData& rtc) {
    enqueueIrrigationEvent(event, state, rtc);
}
