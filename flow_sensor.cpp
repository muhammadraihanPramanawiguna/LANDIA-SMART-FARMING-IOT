
#include "flow_sensor.h"
#include "config.h"

#include <math.h>

// =====================================================
// INTERNAL VARIABLES
// =====================================================

// Jumlah pulsa yang dihitung oleh interrupt.
static volatile unsigned long pulseCount = 0;

// Jumlah pulsa pada interval pengukuran terakhir.
static unsigned long lastPulseCount = 0;

// Akumulasi seluruh pulsa sejak total di-reset.
static unsigned long totalPulseCount = 0;

// Waktu pengukuran terakhir.
static unsigned long lastFlowMillis = 0;

// Hasil pengukuran.
static float flowRateLMin = 0.0f;
static float totalLitres = 0.0f;

// Status validitas data.
static bool flowDataValid = false;

// ESP32 critical section untuk melindungi akses
// variabel pulseCount antara ISR dan program utama.
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

    pinMode(FLOW_PIN, INPUT_PULLUP);

    portENTER_CRITICAL(&flowMux);

    pulseCount = 0;

    portEXIT_CRITICAL(&flowMux);

    lastPulseCount = 0;
    totalPulseCount = 0;

    flowRateLMin = 0.0f;
    totalLitres = 0.0f;

    lastFlowMillis = millis();

    flowDataValid = false;

    attachInterrupt(
        digitalPinToInterrupt(FLOW_PIN),
        flowPulseCounter,
        FALLING
    );

    Serial.println("[FLOW] Sensor initialized");

    Serial.print("[FLOW] Calibration factor: ");

    Serial.print(FLOW_CALIBRATION_FACTOR, 2);

    Serial.println(" pulses/liter");

    if (!isValidCalibrationFactor()) {

        Serial.println(
            "[FLOW] ERROR: Invalid calibration factor"
        );

    }
}


// =====================================================
// UPDATE FLOW
// =====================================================

void flowUpdate() {

    const unsigned long currentMillis = millis();

    const unsigned long elapsedMillis =
        currentMillis - lastFlowMillis;

    // Pengukuran dilakukan minimal setiap 1 detik.
    // Jika pemanggilan terlambat, waktu aktual tetap digunakan.

    if (elapsedMillis < 1000UL) {
        return;
    }

    // -------------------------------------------------
    // VALIDATE CALIBRATION
    // -------------------------------------------------

    if (!isValidCalibrationFactor()) {

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

    // Calibration factor:
    // pulses per liter.
    //
    // Flow rate:
    // (pulses * 60000) /
    // (calibration factor * elapsed milliseconds)

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

    totalPulseCount += pulses;

    // -------------------------------------------------
    // CALCULATE TOTAL VOLUME
    // -------------------------------------------------

    totalLitres =
        static_cast<float>(totalPulseCount) /
        FLOW_CALIBRATION_FACTOR;

    // -------------------------------------------------
    // VALIDATE RESULT
    // -------------------------------------------------

    if (
        !isfinite(flowRateLMin) ||
        !isfinite(totalLitres) ||
        flowRateLMin < 0.0f ||
        totalLitres < 0.0f
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
}


// =====================================================
// READ DATA
// =====================================================

FlowData flowRead() {

    FlowData data = {};

    data.pulseCount = lastPulseCount;

    data.flowRateLMin = flowRateLMin;

    data.totalLitres = totalLitres;

    data.totalCubicMeter =
        totalLitres / 1000.0f;

    data.totalCost =
        totalLitres * WATER_COST_PER_LITER;

    data.valid = flowDataValid;

    return data;
}


// =====================================================
// RESET TOTAL
// =====================================================

void flowResetTotal() {

    // Reset pulsa yang sedang menunggu dibaca.
    portENTER_CRITICAL(&flowMux);

    pulseCount = 0;

    portEXIT_CRITICAL(&flowMux);

    lastPulseCount = 0;

    totalPulseCount = 0;

    totalLitres = 0.0f;

    flowRateLMin = 0.0f;

    lastFlowMillis = millis();

    flowDataValid = false;

    Serial.println("[FLOW] Total usage reset");
}
