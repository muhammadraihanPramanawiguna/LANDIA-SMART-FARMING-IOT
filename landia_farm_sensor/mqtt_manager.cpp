
#include "mqtt_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "offline_storage.h"
#include "secrets.h"

// ============================================================
// LANDIA FARM - MQTT MANAGER
//
// Alur telemetry:
// Sensor -> LittleFS -> MQTT -> Backend -> Application ACK
//
// Kontrol irigasi lokal tetap berjalan di ESP32.
// Koneksi Wi-Fi, MQTT, dan ACK tidak boleh menjadi syarat
// untuk menjalankan kontrol irigasi lokal.
// ============================================================

// ============================================================
// MQTT BROKER
// ============================================================

// Pertahankan alamat broker dari konfigurasi kamu.
// Pastikan alamat ini memang dapat dijangkau ESP32.
const char* MQTT_SERVER = "192.168.0.190";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_CLIENT_ID = "landia-esp32-01";
const char* DEVICE_ID = "landia-esp32-01";

// ============================================================
// MQTT TOPICS
// ============================================================

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

const char* TOPIC_ACK =
    "landia/ack/landia-esp32-01";

// ============================================================
// MQTT OBJECTS AND INTERNAL STATE
// ============================================================

namespace {

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ------------------------------------------------------------
// TIMING
// ------------------------------------------------------------

const unsigned long WIFI_RECONNECT_INTERVAL = 10000UL;
const unsigned long MQTT_RECONNECT_INTERVAL = 10000UL;

const unsigned long OFFLINE_TELEMETRY_INTERVAL = 300000UL;
const unsigned long BACKLOG_SEND_INTERVAL = 500UL;
const unsigned long ACK_TIMEOUT_INTERVAL = 10000UL;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastOfflineTelemetryStore = 0;
unsigned long lastBacklogSend = 0;
unsigned long lastRecordPublish = 0;

bool storageReady = false;

// ACK subscription wajib aktif sebelum telemetry dikirim.
// Tanpa subscription, ESP32 tidak akan menerima konfirmasi backend.
bool ackSubscriptionReady = false;
bool ackSubscribeAttempted = false;
unsigned long lastAckSubscribeAttempt = 0;
const unsigned long ACK_SUBSCRIBE_INTERVAL = 5000UL;

// Satu record dikirim dan menunggu application ACK.
String inFlightRecordId;
String inFlightSensorType;

// ------------------------------------------------------------
// TIMESTAMP
// ------------------------------------------------------------

String getBackendTimestamp() {

    String timestamp = rtcGetTimestamp();

    if (timestamp.length() == 0 ||
        timestamp == "INVALID") {
        return "";
    }

    // Backend menggunakan LocalDateTime:
    // yyyy-MM-ddTHH:mm:ss
    timestamp.replace(' ', 'T');

    return timestamp;
}

// ------------------------------------------------------------
// SENSOR TYPE FROM MQTT TOPIC
// ------------------------------------------------------------

const char* getSensorTypeFromTopic(const char* topic) {

    if (topic == nullptr) {
        return "";
    }

    if (strcmp(topic, TOPIC_DHT) == 0) {
        return "dht";
    }

    if (strcmp(topic, TOPIC_SOIL) == 0) {
        return "soil";
    }

    if (strcmp(topic, TOPIC_WATER_LEVEL) == 0) {
        return "water-level";
    }

    if (strcmp(topic, TOPIC_RAIN) == 0) {
        return "rain";
    }

    if (strcmp(topic, TOPIC_FLOW) == 0) {
        return "flow";
    }

    return "";
}

// ------------------------------------------------------------
// GENERATE RECORD ID
// ------------------------------------------------------------

bool generateRecordId(char* buffer, size_t size) {

    if (buffer == nullptr || size == 0) {
        return false;
    }

    return offlineStorageGenerateRecordId(buffer, size);
}

// ------------------------------------------------------------
// STORE SENSOR PAYLOAD
// ------------------------------------------------------------

bool enqueueSensorPayload(
    const char* topic,
    const char* payload,
    const char* recordId
) {

    if (!storageReady ||
        topic == nullptr ||
        payload == nullptr ||
        recordId == nullptr) {
        return false;
    }

    bool saved = offlineStorageEnqueue(
        recordId,
        topic,
        payload,
        false
    );

    if (!saved) {
        Serial.printf(
            "[OFFLINE] Gagal menyimpan record: %s\n",
            topic
        );

        return false;
    }

    Serial.printf(
        "[OFFLINE] Record tersimpan: %s | %s\n",
        getSensorTypeFromTopic(topic),
        recordId
    );

    return true;
}

// ------------------------------------------------------------
// CREATE AND STORE ONE SENSOR RECORD
// ------------------------------------------------------------

bool queueTelemetrySet(
    const DHTData& dht,
    const SoilData& soil,
    const RainData& rain,
    const FlowData& flow,
    const WaterLevelData& waterLevel
) {

    char recordId[41];
    char payload[512];

    String timestamp = getBackendTimestamp();

    bool allSaved = true;
    int length = 0;

    // ========================================================
    // 1. DHT SENSOR
    // ========================================================

    if (dht.valid &&
        isfinite(dht.temperature) &&
        isfinite(dht.humidity)) {

        if (!generateRecordId(recordId, sizeof(recordId))) {
            allSaved = false;
        } else {

            if (timestamp.length() > 0) {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-dht-01\","
                    "\"temperature\":%.2f,"
                    "\"humidity\":%.2f,"
                    "\"recordedAt\":\"%s\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    dht.temperature,
                    dht.humidity,
                    timestamp.c_str()
                );

            } else {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-dht-01\","
                    "\"temperature\":%.2f,"
                    "\"humidity\":%.2f"
                    "}",
                    recordId,
                    DEVICE_ID,
                    dht.temperature,
                    dht.humidity
                );
            }

            if (length <= 0 ||
                length >= (int)sizeof(payload) ||
                !enqueueSensorPayload(
                    TOPIC_DHT,
                    payload,
                    recordId
                )) {
                allSaved = false;
            }
        }

    } else {
        Serial.println("[SENSOR] DHT tidak valid, dilewati.");
    }


    // ========================================================
    // 2. SOIL MOISTURE SENSOR
    // ========================================================

    if (soil.valid && isfinite(soil.moisturePercent)) {

        if (!generateRecordId(recordId, sizeof(recordId))) {
            allSaved = false;
        } else {

            if (timestamp.length() > 0) {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-soil-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\","
                    "\"recordedAt\":\"%s\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    soil.moisturePercent,
                    timestamp.c_str()
                );

            } else {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-soil-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    soil.moisturePercent
                );
            }

            if (length <= 0 ||
                length >= (int)sizeof(payload) ||
                !enqueueSensorPayload(
                    TOPIC_SOIL,
                    payload,
                    recordId
                )) {

                allSaved = false;
            }
        }

    } else {
        Serial.println("[SENSOR] Soil tidak valid, dilewati.");
    }

    // ========================================================
    // 3. RAIN SENSOR
    // ========================================================

    if (rain.valid && isfinite(rain.rainPercent)) {

        if (!generateRecordId(recordId, sizeof(recordId))) {
            allSaved = false;
        } else {

            if (timestamp.length() > 0) {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-rain-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\","
                    "\"recordedAt\":\"%s\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    rain.rainPercent,
                    timestamp.c_str()
                );

            } else {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-rain-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    rain.rainPercent
                );
            }

            if (length <= 0 ||
                length >= (int)sizeof(payload) ||
                !enqueueSensorPayload(
                    TOPIC_RAIN,
                    payload,
                    recordId
                )) {
                allSaved = false;
            }
        }

    } else {
        Serial.println("[SENSOR] Rain tidak valid, dilewati.");
    }

    // ========================================================
    // 4. FLOW SENSOR
    // ========================================================

    if (flow.valid && isfinite(flow.flowRateLMin)) {

        if (!generateRecordId(recordId, sizeof(recordId))) {
            allSaved = false;
        } else {

            if (timestamp.length() > 0) {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-flow-01\","
                    "\"value\":%.3f,"
                    "\"unit\":\"L/min\","
                    "\"recordedAt\":\"%s\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    flow.flowRateLMin,
                    timestamp.c_str()
                );

            } else {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-flow-01\","
                    "\"value\":%.3f,"
                    "\"unit\":\"L/min\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    flow.flowRateLMin
                );
            }

            if (length <= 0 ||
                length >= (int)sizeof(payload) ||
                !enqueueSensorPayload(
                    TOPIC_FLOW,
                    payload,
                    recordId
                )) {
                allSaved = false;
            }
        }

    } else {
        Serial.println("[SENSOR] Flow tidak valid, dilewati.");
    }

    // ========================================================
    // 5. WATER LEVEL SENSOR
    // ========================================================

    if (waterLevel.valid &&
        isfinite(waterLevel.waterLevelPercent)) {

        if (!generateRecordId(recordId, sizeof(recordId))) {
            allSaved = false;
        } else {

            if (timestamp.length() > 0) {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-water-level-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\","
                    "\"recordedAt\":\"%s\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    waterLevel.waterLevelPercent,
                    timestamp.c_str()
                );

            } else {

                length = snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"recordId\":\"%s\","
                    "\"deviceId\":\"%s\","
                    "\"sensorId\":\"landia-water-level-01\","
                    "\"value\":%.2f,"
                    "\"unit\":\"%%\""
                    "}",
                    recordId,
                    DEVICE_ID,
                    waterLevel.waterLevelPercent
                );
            }

            if (length <= 0 ||
                length >= (int)sizeof(payload) ||
                !enqueueSensorPayload(
                    TOPIC_WATER_LEVEL,
                    payload,
                    recordId
                )) {
                allSaved = false;
            }
        }

    } else {
        Serial.println("[SENSOR] Water level tidak valid, dilewati.");
    }

    if (allSaved) {
        Serial.println(
            "[OFFLINE] Seluruh telemetry valid berhasil diproses."
        );
    } else {
        Serial.println(
            "[OFFLINE] Ada telemetry yang gagal disimpan."
        );
    }

    return allSaved;
}

// ------------------------------------------------------------
// APPLICATION ACK CALLBACK
// ------------------------------------------------------------

void mqttCallback(
    char* topic,
    byte* payload,
    unsigned int length
) {

    if (topic == nullptr ||
        payload == nullptr ||
        strcmp(topic, TOPIC_ACK) != 0) {
        return;
    }

    // ArduinoJson 7
    JsonDocument document;

    DeserializationError error = deserializeJson(
        document,
        payload,
        length
    );

    if (error) {
        Serial.printf(
            "[ACK] JSON tidak valid: %s\n",
            error.c_str()
        );

        return;
    }

    const char* recordId =
        document["recordId"] | "";

    const char* deviceId =
        document["deviceId"] | "";

    const char* sensorType =
        document["sensorType"] | "";

    const char* status =
        document["status"] | "";

    if (recordId[0] == '\0' ||
        deviceId[0] == '\0' ||
        sensorType[0] == '\0' ||
        status[0] == '\0') {

        Serial.println("[ACK] Field ACK tidak lengkap.");
        return;
    }

    // Pastikan ACK berasal untuk device ini.
    if (strcmp(deviceId, DEVICE_ID) != 0) {
        Serial.println("[ACK] Device ID tidak cocok.");
        return;
    }

    // Hanya status stored yang mengizinkan penghapusan.
    if (strcmp(status, "stored") != 0) {
        Serial.printf(
            "[ACK] Status bukan stored: %s\n",
            status
        );

        return;
    }

    // Harus ada record yang sedang menunggu ACK.
    if (inFlightRecordId.length() == 0) {
        Serial.println("[ACK] Tidak ada record yang menunggu ACK.");
        return;
    }

    // Record ID wajib sama.
    if (inFlightRecordId != recordId) {
        Serial.printf(
            "[ACK] Record ID tidak cocok. Diterima: %s\n",
            recordId
        );

        return;
    }

    // Sensor type wajib sesuai dengan topic record aktif.
    if (inFlightSensorType != sensorType) {
        Serial.printf(
            "[ACK] Sensor type tidak cocok. Diharapkan: %s, diterima: %s\n",
            inFlightSensorType.c_str(),
            sensorType
        );

        return;
    }

    // Hapus hanya setelah seluruh validasi ACK berhasil.
    if (!offlineStorageRemove(recordId)) {
        Serial.printf(
            "[ACK] Gagal menghapus record: %s\n",
            recordId
        );

        return;
    }

    Serial.printf(
        "[ACK] Backend confirmed: %s | %s\n",
        recordId,
        sensorType
    );

    inFlightRecordId = "";
    inFlightSensorType = "";

    lastRecordPublish = 0;
}

// ------------------------------------------------------------
// WIFI RECONNECT
// ------------------------------------------------------------

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
        "[WIFI] Koneksi terputus. Mencoba reconnect..."
    );

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );
}

// ------------------------------------------------------------
// ENSURE ACK SUBSCRIPTION
// ------------------------------------------------------------

bool ensureAckSubscription() {

    if (!mqttClient.connected()) {
        ackSubscriptionReady = false;
        return false;
    }

    if (ackSubscriptionReady) {
        return true;
    }

    const unsigned long now = millis();

    if (ackSubscribeAttempted &&
        now - lastAckSubscribeAttempt < ACK_SUBSCRIBE_INTERVAL) {
        return false;
    }

    ackSubscribeAttempted = true;
    lastAckSubscribeAttempt = now;

    Serial.printf(
        "[MQTT] Mencoba subscribe ACK: %s\\n",
        TOPIC_ACK
    );

    if (mqttClient.subscribe(TOPIC_ACK, 1)) {
        ackSubscriptionReady = true;

        Serial.printf(
            "[MQTT] ACK subscription berhasil: %s\\n",
            TOPIC_ACK
        );

        return true;
    }

    ackSubscriptionReady = false;
    Serial.println("[MQTT] Subscribe ACK gagal. Akan dicoba ulang.");
    return false;
}

// ------------------------------------------------------------
// MQTT CONNECT
// ------------------------------------------------------------

void mqttConnect() {

    if (WiFi.status() != WL_CONNECTED ||
        mqttClient.connected()) {
        return;
    }

    const unsigned long now = millis();

    // Percobaan pertama boleh langsung dilakukan; berikutnya diberi jeda.
    static bool connectionAttempted = false;

    if (connectionAttempted &&
        now - lastMQTTAttempt < MQTT_RECONNECT_INTERVAL) {
        return;
    }

    connectionAttempted = true;
    lastMQTTAttempt = now;

    Serial.println("[MQTT] Connecting to broker...");

    if (mqttClient.connect(MQTT_CLIENT_ID)) {

        Serial.println("[MQTT] Connected.");

        // Status subscription selalu dianggap belum siap setelah connect.
        ackSubscriptionReady = false;
        ackSubscribeAttempted = false;
        lastAckSubscribeAttempt = 0;

        // Record yang belum mendapat ACK akan dikirim ulang.
        inFlightRecordId = "";
        inFlightSensorType = "";

        lastRecordPublish = 0;
        lastBacklogSend = 0;

        // Langsung coba subscribe. Jika gagal, mqttUpdate akan retry.
        ensureAckSubscription();

    } else {

        ackSubscriptionReady = false;

        Serial.printf(
            "[MQTT] Connection failed. State=%d\\n",
            mqttClient.state()
        );
    }
}

// ------------------------------------------------------------
// SEND BACKLOG
// ------------------------------------------------------------

bool publishOneBacklogRecord() {

    if (!mqttClient.connected() ||
        !storageReady ||
        !ackSubscriptionReady) {
        return false;
    }

    unsigned long now = millis();

    // Jika masih menunggu ACK, jangan kirim record lain.
    if (inFlightRecordId.length() > 0) {

        if (now - lastRecordPublish <
            ACK_TIMEOUT_INTERVAL) {
            return false;
        }

        Serial.printf(
            "[MQTT] ACK timeout. Record akan dicoba ulang: %s\n",
            inFlightRecordId.c_str()
        );

        // Record tidak dihapus dari LittleFS.
        // Kosongkan status in-flight agar record yang sama
        // dapat dikirim ulang.
        inFlightRecordId = "";
        inFlightSensorType = "";
    }

    if (now - lastBacklogSend <
        BACKLOG_SEND_INTERVAL) {
        return false;
    }

    OfflineRecord record;

    // Ambil hanya antrean telemetry. Antrean event memiliki jalur
    // terpisah agar event yang rusak/tidak terbaca tidak memblokir sensor.
    if (!offlineStoragePeekTelemetry(record)) {
        return false;
    }

    if (record.isEvent) {
        Serial.println(
            "[MQTT] ERROR: record event ditemukan pada antrean telemetry."
        );
        return false;
    }

    const char* sensorType =
        getSensorTypeFromTopic(record.topic.c_str());

    if (sensorType[0] == '\0') {
        Serial.printf(
            "[MQTT] Topic tidak dikenal: %s\n",
            record.topic.c_str()
        );

        return false;
    }

    if (!mqttClient.publish(
            record.topic.c_str(),
            record.payload.c_str(),
            false
        )) {

        Serial.printf(
            "[MQTT] Publish gagal: %s\n",
            record.topic.c_str()
        );

        return false;
    }

    // Publish MQTT berhasil bukan berarti database backend
    // sudah menyimpan data.
    // Record tetap di LittleFS sampai application ACK valid.
    inFlightRecordId = record.recordId;
    inFlightSensorType = sensorType;

    lastRecordPublish = now;
    lastBacklogSend = now;

    Serial.printf(
        "[MQTT] Record terkirim. Menunggu ACK: %s | %s\n",
        record.recordId.c_str(),
        sensorType
    );

    return true;
}

} // namespace

// ============================================================
// MQTT INIT
// ============================================================

void mqttInit() {

    Serial.println();
    Serial.println("========== LANDIA MQTT INIT ==========");

    storageReady = offlineStorageBegin();

    if (!storageReady) {

        Serial.println(
            "[OFFLINE] LittleFS tidak tersedia. "
            "Telemetry tidak dapat diantrekan."
        );
    } else {

        Serial.println("[OFFLINE] LittleFS siap.");
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    mqttClient.setServer(
        MQTT_SERVER,
        MQTT_PORT
    );

    mqttClient.setCallback(mqttCallback);

    mqttClient.setSocketTimeout(2);
    mqttClient.setKeepAlive(15);
    mqttClient.setBufferSize(768);

    Serial.println("[MQTT] Manager initialized.");
}

// ============================================================
// CONNECTION STATUS
// ============================================================

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool mqttIsConnected() {
    return mqttClient.connected();
}

// ============================================================
// MQTT UPDATE
// ============================================================

void mqttUpdate() {

    if (!wifiIsConnected()) {
        ackSubscriptionReady = false;
        mqttEnsureWiFi();
        return;
    }

    if (!mqttClient.connected()) {
        ackSubscriptionReady = false;
        mqttConnect();
        return;
    }

    // Proses ACK dan pesan MQTT masuk.
    mqttClient.loop();

    // Jangan mengirim telemetry sebelum ACK topic berhasil disubscribe.
    if (!ensureAckSubscription()) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastBacklogSend >= BACKLOG_SEND_INTERVAL) {
        publishOneBacklogRecord();
    }
}

// ============================================================
// PUBLISH TELEMETRY
// ============================================================

void mqttPublishTelemetry(
    const DHTData& dht,
    const SoilData& soil,
    const RainData& rain,
    const FlowData& flow,
    const WaterLevelData& waterLevel,
    const RTCData& rtc
) {

    (void)rtc;

    if (!storageReady) {

        Serial.println(
            "[OFFLINE] Telemetry dilewati: storage unavailable."
        );

        return;
    }

    // Ketika broker offline, simpan satu set telemetry
    // setiap lima menit agar LittleFS tidak cepat penuh.
    if (!mqttClient.connected()) {

        unsigned long now = millis();

        if (lastOfflineTelemetryStore != 0 &&
            now - lastOfflineTelemetryStore <
                OFFLINE_TELEMETRY_INTERVAL) {
            return;
        }

        lastOfflineTelemetryStore = now;
    }

    // Semua data valid disimpan ke LittleFS terlebih dahulu.
    // Pengiriman dilakukan terpisah oleh mqttUpdate().
    queueTelemetrySet(
        dht,
        soil,
        rain,
        flow,
        waterLevel
    );
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

    if (event == nullptr || state == nullptr ||
        event[0] == '\0' || state[0] == '\0') {
        Serial.println("[EVENT] Event/state kosong; record dilewati.");
        return;
    }

    if (!storageReady) {
        Serial.println(
            "[EVENT] LittleFS tidak tersedia. Event tidak dapat disimpan."
        );
        return;
    }

    char recordId[41];
    char payload[384];

    if (!generateRecordId(recordId, sizeof(recordId))) {
        Serial.println("[EVENT] Gagal membuat recordId.");
        return;
    }

    String timestamp = getBackendTimestamp();
    int length = 0;

    if (timestamp.length() > 0) {
        length = snprintf(
            payload,
            sizeof(payload),
            "{"
            "\"recordId\":\"%s\","
            "\"deviceId\":\"%s\","
            "\"event\":\"%s\","
            "\"state\":\"%s\","
            "\"timestamp\":\"%s\""
            "}",
            recordId,
            DEVICE_ID,
            event,
            state,
            timestamp.c_str()
        );
    } else {
        length = snprintf(
            payload,
            sizeof(payload),
            "{"
            "\"recordId\":\"%s\","
            "\"deviceId\":\"%s\","
            "\"event\":\"%s\","
            "\"state\":\"%s\""
            "}",
            recordId,
            DEVICE_ID,
            event,
            state
        );
    }

    if (length <= 0 || length >= (int)sizeof(payload)) {
        Serial.println("[EVENT] Payload terlalu panjang/gagal dibuat.");
        return;
    }

    // Snapshot backend yang diberikan belum memiliki consumer MQTT
    // untuk landia/irrigation/event maupun application ACK event.
    // Karena itu event disimpan dulu dan TIDAK dihapus hanya karena
    // broker menerima publish QoS 0. Ini mencegah kehilangan histori.
    if (!offlineStorageEnqueue(
            recordId,
            TOPIC_IRRIGATION_EVENT,
            payload,
            true
        )) {
        Serial.println("[EVENT] Gagal menyimpan event ke LittleFS.");
        return;
    }

    Serial.printf(
        "[EVENT] Tersimpan di LittleFS: %s | %s | %s\n",
        recordId,
        event,
        state
    );

    if (mqttClient.connected()) {
        Serial.println(
            "[EVENT] MQTT online, tetapi pengiriman ditahan sampai backend "
            "menyediakan consumer + ACK event."
        );
    } else {
        Serial.println("[EVENT] MQTT offline; event aman di antrean lokal.");
    }
}
