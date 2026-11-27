
#include "flow_sensor.h"
#include "config.h"

#include <Preferences.h>
#include <math.h>
#include <stdint.h>

// =====================================================
// NVS CONFIGURATION
// =====================================================

static const char* FLOW_NVS_NAMESPACE = "landia_flow";
static const char* FLOW_NVS_KEY = "total_pulses";

// Preferences untuk penyimpanan permanen ESP32.
static Preferences flowPreferences;

static bool flowStorageReady = false;

// =====================================================
// INTERNAL VARIABLES
// =====================================================

// Pulsa yang dihitung oleh interrupt.
static volatile unsigned long pulseCount = 0;

// Pulsa pada interval pengukuran terakhir.
static unsigned long lastPulseCount = 0;

// Total pulsa kumulatif.
static uint64_t totalPulseCount = 0;

// Total pulsa yang terakhir berhasil disimpan ke NVS.
static uint64_t lastPersistedPulseCount = 0;

// Waktu pengukuran terakhir.
static unsigned long lastFlowMillis = 0;

// Waktu penyimpanan terakhir.
static unsigned long lastPersistMillis = 0;

// Hasil pengukuran.
static float flowRateLMin = 0.0f;

// Volume internal menggunakan double agar
// presisi lebih baik untuk akumulasi jangka panjang.
static double totalLitres = 0.0;

// Status validitas data.
static bool flowDataValid = false;

// ESP32 critical section untuk melindungi
// akses variabel pulsa antara ISR dan program utama.
static portMUX_TYPE flowMux =
    portMUX_INITIALIZER_UNLOCKED;


// =====================================================
// CALIBRATION VALIDATION
// =====================================================

static bool isValidCalibrationFactor() {

    return
        isfinite(FLOW_CALIBRATION_FACTOR) &&
        FLOW_CALIBRATION_FACTOR > 0.0f;
}


// =====================================================
// TARIFF VALIDATION
// =====================================================

static bool isValidWaterTariff() {

    return
        isfinite(WATER_TARIFF_PER_M3) &&
        WATER_TARIFF_PER_M3 >= 0.0f &&
        isfinite(LITERS_PER_CUBIC_METER) &&
        LITERS_PER_CUBIC_METER > 0.0f;
}


// =====================================================
// PERSIST TOTAL PULSE TO NVS
// =====================================================

static bool persistTotalPulseCount(bool force) {

    if (!flowStorageReady) {
        return false;
    }

    uint64_t pulseSnapshot = 0;

    // Ambil snapshot total pulsa secara aman.
    portENTER_CRITICAL(&flowMux);

    pulseSnapshot = totalPulseCount;

    portEXIT_CRITICAL(&flowMux);

    // Tidak perlu menulis jika tidak ada perubahan.
    if (
        !force &&
        pulseSnapshot == lastPersistedPulseCount
    ) {
        return true;
    }

    // Simpan total pulsa ke NVS.
    size_t written = flowPreferences.putULong64(
        FLOW_NVS_KEY,
        pulseSnapshot
    );

    if (written != sizeof(uint64_t)) {

        Serial.println(
            "[FLOW] ERROR: Failed to persist total pulses"
        );

        return false;
    }

    lastPersistedPulseCount = pulseSnapshot;
    lastPersistMillis = millis();

    Serial.print("[FLOW] NVS checkpoint saved: ");

    Serial.print(
        static_cast<unsigned long long>(pulseSnapshot)
    );

    Serial.println(" pulses");

    return true;
}


// =====================================================
// CHECKPOINT MANAGEMENT
// =====================================================

static void flowPersistenceUpdate() {

    if (!flowStorageReady) {
        return;
    }

    const unsigned long currentMillis = millis();

    uint64_t currentPulseSnapshot = 0;

    portENTER_CRITICAL(&flowMux);

    currentPulseSnapshot = totalPulseCount;

    portEXIT_CRITICAL(&flowMux);

    // Tidak ada data baru yang perlu disimpan.
    if (
        currentPulseSnapshot ==
        lastPersistedPulseCount
    ) {
        return;
    }

    // Jika total RAM lebih kecil dari checkpoint sebelumnya,
    // kemungkinan terjadi reset total yang belum tersimpan.
    // Hindari unsigned underflow pada perhitungan selisih.
    if (currentPulseSnapshot < lastPersistedPulseCount) {
        persistTotalPulseCount(true);
        return;
    }

    // Hitung pertambahan pulsa sejak checkpoint terakhir.
    const uint64_t pulseDifference =
        currentPulseSnapshot - lastPersistedPulseCount;

    // Konversi ambang volume ke jumlah pulsa.
    const double thresholdPulses =
        static_cast<double>(FLOW_PERSIST_VOLUME_THRESHOLD_L) *
        static_cast<double>(FLOW_CALIBRATION_FACTOR);

    const bool volumeThresholdReached =
        isfinite(thresholdPulses) &&
        thresholdPulses > 0.0 &&
        static_cast<double>(pulseDifference) >= thresholdPulses;

    // Checkpoint berdasarkan waktu.
    const bool timeThresholdReached =
        (
            currentMillis - lastPersistMillis
        ) >= FLOW_PERSIST_INTERVAL_MS;

    // Simpan jika ambang volume ATAU waktu tercapai.
    if (
        volumeThresholdReached ||
        timeThresholdReached
    ) {

        persistTotalPulseCount(false);

    }
}


// =====================================================
// INTERRUPT
// =====================================================

void IRAM_ATTR flowPulseCounter() {

    portENTER_CRITICAL_ISR(&flowMux);

    pulseCount++;

    portEXIT_CRITICAL_ISR(&flowMux);
}


// =====================================================
// INITIALIZATION
// =====================================================

void flowInit() {

    Serial.println();
    Serial.println("[FLOW] Initializing flow sensor...");

    // -------------------------------------------------
    // RESET RUNTIME VARIABLES
    // -------------------------------------------------

    pinMode(FLOW_PIN, INPUT_PULLUP);

    portENTER_CRITICAL(&flowMux);

    pulseCount = 0;
    totalPulseCount = 0;

    portEXIT_CRITICAL(&flowMux);

    lastPulseCount = 0;
    lastPersistedPulseCount = 0;

    flowRateLMin = 0.0f;
    totalLitres = 0.0;

    lastFlowMillis = millis();
    lastPersistMillis = lastFlowMillis;

    flowDataValid = false;

    // -------------------------------------------------
    // INITIALIZE NVS
    // -------------------------------------------------

    flowStorageReady = flowPreferences.begin(
        FLOW_NVS_NAMESPACE,
        false
    );

    if (flowStorageReady) {

        // Pulihkan total pulsa dari penyimpanan sebelumnya.
        totalPulseCount = flowPreferences.getULong64(
            FLOW_NVS_KEY,
            0ULL
        );

        lastPersistedPulseCount = totalPulseCount;

        // Pulihkan volume kumulatif.
        if (isValidCalibrationFactor()) {

            totalLitres =
                static_cast<double>(totalPulseCount) /
                static_cast<double>(FLOW_CALIBRATION_FACTOR);

        }

        Serial.println("[FLOW] NVS storage initialized");

        Serial.print("[FLOW] Restored total pulses: ");

        Serial.println(
            static_cast<unsigned long long>(totalPulseCount)
        );

        Serial.print("[FLOW] Restored total volume: ");

        Serial.print(totalLitres, 3);

        Serial.println(" L");

    } else {

        Serial.println(
            "[FLOW] WARNING: NVS initialization failed"
        );

        Serial.println(
            "[FLOW] Running without persistent storage"
        );

    }

    // -------------------------------------------------
    // VALIDATE CALIBRATION
    // -------------------------------------------------

    if (!isValidCalibrationFactor()) {

        Serial.println(
            "[FLOW] ERROR: Invalid calibration factor"
        );

    } else {

        Serial.print("[FLOW] Calibration factor: ");

        Serial.print(FLOW_CALIBRATION_FACTOR, 2);

        Serial.println(" pulses/liter");

    }

    // -------------------------------------------------
    // VALIDATE TARIFF
    // -------------------------------------------------

    if (!isValidWaterTariff()) {

        Serial.println(
            "[FLOW] ERROR: Invalid water tariff configuration"
        );

    } else {

        Serial.print("[FLOW] Water tariff: Rp ");

        Serial.print(WATER_TARIFF_PER_M3, 2);

        Serial.println("/m3");

    }

    // -------------------------------------------------
    // ATTACH INTERRUPT
    // -------------------------------------------------

    attachInterrupt(
        digitalPinToInterrupt(FLOW_PIN),
        flowPulseCounter,
        FALLING
    );

    Serial.println("[FLOW] Sensor initialized");
    Serial.println();

}


// =====================================================
// UPDATE FLOW
// =====================================================

void flowUpdate() {

    const unsigned long currentMillis = millis();

    const unsigned long elapsedMillis =
        currentMillis - lastFlowMillis;

    // Pengukuran minimal setiap 1 detik.
    if (elapsedMillis < 1000UL) {
        return;
    }

    // -------------------------------------------------
    // VALIDATE CALIBRATION
    // -------------------------------------------------

    if (!isValidCalibrationFactor()) {

        portENTER_CRITICAL(&flowMux);

        pulseCount = 0;

        portEXIT_CRITICAL(&flowMux);

        lastPulseCount = 0;

        flowRateLMin = 0.0f;
        flowDataValid = false;

        lastFlowMillis = currentMillis;

        return;
    }

    // -------------------------------------------------
    // READ PULSE SAFELY
    // -------------------------------------------------

    unsigned long pulses = 0;

    portENTER_CRITICAL(&flowMux);

    pulses = pulseCount;

    pulseCount = 0;

    portEXIT_CRITICAL(&flowMux);

    lastPulseCount = pulses;

    // -------------------------------------------------
    // CALCULATE FLOW RATE
    // -------------------------------------------------

    flowRateLMin =
        (
            static_cast<float>(pulses) *
            60000.0f
        ) /
        (
            FLOW_CALIBRATION_FACTOR *
            static_cast<float>(elapsedMillis)
        );

    // -------------------------------------------------
    // UPDATE TOTAL PULSE
    // -------------------------------------------------

    if (pulses > 0) {

        portENTER_CRITICAL(&flowMux);

        totalPulseCount += static_cast<uint64_t>(pulses);

        portEXIT_CRITICAL(&flowMux);

    }

    // -------------------------------------------------
    // CALCULATE TOTAL VOLUME
    // -------------------------------------------------

    uint64_t pulseSnapshot = 0;

    portENTER_CRITICAL(&flowMux);

    pulseSnapshot = totalPulseCount;

    portEXIT_CRITICAL(&flowMux);

    totalLitres =
        static_cast<double>(pulseSnapshot) /
        static_cast<double>(FLOW_CALIBRATION_FACTOR);

    // -------------------------------------------------
    // VALIDATE RESULT
    // -------------------------------------------------

    if (
        !isfinite(flowRateLMin) ||
        !isfinite(totalLitres) ||
        flowRateLMin < 0.0f ||
        totalLitres < 0.0
    ) {

        flowRateLMin = 0.0f;
        flowDataValid = false;

        lastFlowMillis = currentMillis;

        return;
    }

    // -------------------------------------------------
    // UPDATE STATUS
    // -------------------------------------------------

    flowDataValid = true;

    lastFlowMillis = currentMillis;

    // -------------------------------------------------
    // NVS CHECKPOINT
    // -------------------------------------------------

    flowPersistenceUpdate();

}


// =====================================================
// READ DATA
// =====================================================

FlowData flowRead() {

    FlowData data = {};

    data.pulseCount = lastPulseCount;

    portENTER_CRITICAL(&flowMux);

    data.totalPulseCount = totalPulseCount;

    portEXIT_CRITICAL(&flowMux);

    data.flowRateLMin = flowRateLMin;

    data.totalLitres = static_cast<float>(totalLitres);

    // Hindari pembagian dengan konstanta volume yang invalid.
    if (
        isfinite(LITERS_PER_CUBIC_METER) &&
        LITERS_PER_CUBIC_METER > 0.0f
    ) {
        data.totalCubicMeter =
            data.totalLitres / LITERS_PER_CUBIC_METER;
    } else {
        data.totalCubicMeter = 0.0f;
    }

    // Biaya hanya dihitung jika konfigurasi tarif valid.
    if (isValidWaterTariff()) {
        data.totalCost =
            data.totalCubicMeter * WATER_TARIFF_PER_M3;
    } else {
        data.totalCost = 0.0f;
    }

    data.valid = flowDataValid;

    return data;
}


// =====================================================
// RESET TOTAL
// =====================================================

void flowResetTotal() {

    Serial.println("[FLOW] Resetting cumulative water usage...");

    // -------------------------------------------------
    // RESET PULSE COUNTERS
    // -------------------------------------------------

    portENTER_CRITICAL(&flowMux);

    pulseCount = 0;
    totalPulseCount = 0;

    portEXIT_CRITICAL(&flowMux);

    lastPulseCount = 0;

    // Reset baseline checkpoint juga agar tidak terjadi
    // unsigned underflow setelah reset total.
    lastPersistedPulseCount = 0;
    lastPersistMillis = millis();

    // -------------------------------------------------
    // RESET MEASUREMENT
    // -------------------------------------------------

    flowRateLMin = 0.0f;
    totalLitres = 0.0;

    flowDataValid = false;

    lastFlowMillis = millis();

    // -------------------------------------------------
    // PERSIST RESET TO NVS
    // -------------------------------------------------

    if (flowStorageReady) {

        if (persistTotalPulseCount(true)) {

            Serial.println(
                "[FLOW] Total usage reset successfully"
            );

        } else {

            Serial.println(
                "[FLOW] ERROR: Reset could not be saved to NVS"
            );

        }

    } else {

        Serial.println(
            "[FLOW] WARNING: Reset only applied to RAM"
        );

    }

}
