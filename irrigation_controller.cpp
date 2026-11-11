#include <Arduino.h>

#include "config.h"
#include "irrigation_controller.h"
#include "relay_manager.h"

// =====================================================
// INTERNAL STATE
// =====================================================

static bool irrigationActive = false;
static IrrigationState irrigationState = IRRIGATION_IDLE;

// =====================================================
// SESSION DATA
// =====================================================

static float irrigationStartLitres = 0.0f;
static float lastSessionLitres = 0.0f;
static float lastSessionCost = 0.0f;
static unsigned long irrigationStartMillis = 0;

// =====================================================
// FLOW MONITOR
// =====================================================

static unsigned long lastFlowDetectionMillis = 0;

// =====================================================
// DAILY SCHEDULE
// =====================================================

static bool scheduleHandledToday = false;
static long lastScheduleDateKey = -1;

// =====================================================
// INITIALIZATION
// =====================================================

void irrigationInit() {
    irrigationActive = false;
    irrigationState = IRRIGATION_IDLE;

    irrigationStartLitres = 0.0f;
    lastSessionLitres = 0.0f;
    lastSessionCost = 0.0f;

    irrigationStartMillis = 0;
    lastFlowDetectionMillis = 0;

    scheduleHandledToday = false;
    lastScheduleDateKey = -1;

    // SAFETY: POMPA HARUS OFF SAAT BOOT.
    relayOff();

    Serial.println("[IRRIGATION] Controller initialized");
    Serial.println("[IRRIGATION] Pump state: OFF");
}

// =====================================================
// RTC CORRECTION SAFETY
// =====================================================

void irrigationNotifyTimeCorrection(const RTCData& rtc) {
    if (!rtc.valid) {
        Serial.println(
            "[IRRIGATION] RTC correction ignored: invalid RTC data"
        );
        return;
    }

    const long correctedDateKey =
        rtc.year * 10000L +
        rtc.month * 100L +
        rtc.day;

    const int correctedMinutes =
        rtc.hour * 60 + rtc.minute;

    const int scheduleMinutes =
        IRRIGATION_SCHEDULE_HOUR * 60 +
        IRRIGATION_SCHEDULE_MINUTE;

    // Simpan status sebelumnya agar koreksi jam mundur
    // pada tanggal yang sama tidak membuka ulang jadwal.
    const bool sameDate =
        correctedDateKey == lastScheduleDateKey;

    const bool wasAlreadyHandled =
        scheduleHandledToday;

    lastScheduleDateKey = correctedDateKey;

    if (sameDate) {
        scheduleHandledToday =
            wasAlreadyHandled ||
            correctedMinutes >= scheduleMinutes;
    } else {
        // Hari/tanggal hasil koreksi baru:
        // jika jadwal sudah lewat, kunci jadwal hari ini.
        scheduleHandledToday =
            correctedMinutes >= scheduleMinutes;
    }

    Serial.println("[IRRIGATION] RTC correction detected");

    Serial.print("[IRRIGATION] Corrected RTC: ");

    Serial.print(rtc.year);
    Serial.print('-');

    if (rtc.month < 10) Serial.print('0');
    Serial.print(rtc.month);
    Serial.print('-');

    if (rtc.day < 10) Serial.print('0');
    Serial.print(rtc.day);
    Serial.print(' ');

    if (rtc.hour < 10) Serial.print('0');
    Serial.print(rtc.hour);
    Serial.print(':');

    if (rtc.minute < 10) Serial.print('0');
    Serial.println(rtc.minute);

    if (scheduleHandledToday) {
        Serial.println(
            "[IRRIGATION] Today's schedule locked after RTC correction"
        );
    } else {
        Serial.println(
            "[IRRIGATION] Today's scheduled window remains available"
        );
    }
}

// =====================================================
// DATE KEY
// =====================================================

static long getDateKey(const RTCData& rtc) {
    return
        (rtc.year * 10000L) +
        (rtc.month * 100L) +
        rtc.day;
}

// =====================================================
// SCHEDULE CHECK
// =====================================================

static bool isWithinScheduleWindow(const RTCData& rtc) {
    if (!rtc.valid) {
        return false;
    }

    // -------------------------------------------------
    // NEW DAY
    // -------------------------------------------------

    const long currentDateKey = getDateKey(rtc);

    if (currentDateKey != lastScheduleDateKey) {
        lastScheduleDateKey = currentDateKey;
        scheduleHandledToday = false;
    }

    // -------------------------------------------------
    // CURRENT TIME
    // -------------------------------------------------

    const int currentMinutes =
        rtc.hour * 60 + rtc.minute;

    const int scheduleMinutes =
        IRRIGATION_SCHEDULE_HOUR * 60 +
        IRRIGATION_SCHEDULE_MINUTE;

    const int deadlineMinutes =
        scheduleMinutes + IRRIGATION_MAX_DELAY_MINUTES;

    // -------------------------------------------------
    // BEFORE SCHEDULE
    // -------------------------------------------------

    if (currentMinutes < scheduleMinutes) {
        return false;
    }

    // -------------------------------------------------
    // ALREADY HANDLED
    // -------------------------------------------------

    if (scheduleHandledToday) {
        return false;
    }

    // -------------------------------------------------
    // INSIDE WINDOW
    // -------------------------------------------------

    if (currentMinutes <= deadlineMinutes) {
        return true;
    }

    // -------------------------------------------------
    // WINDOW EXPIRED
    // -------------------------------------------------

    scheduleHandledToday = true;
    irrigationState = IRRIGATION_SKIPPED;

    Serial.println("[IRRIGATION] Schedule window expired");

    return false;
}

// =====================================================
// START IRRIGATION
// =====================================================

static void startIrrigation(const FlowData& flow) {
    irrigationActive = true;
    irrigationState = IRRIGATION_IRRIGATING;

    irrigationStartMillis = millis();
    irrigationStartLitres = flow.totalLitres;

    lastSessionLitres = 0.0f;
    lastSessionCost = 0.0f;

    lastFlowDetectionMillis = millis();

    relayOn();

    Serial.println();
    Serial.println("[IRRIGATION] ========================");
    Serial.println("[IRRIGATION] PUMP ON");

    Serial.print("[IRRIGATION] Start total: ");
    Serial.print(irrigationStartLitres, 2);
    Serial.println(" L");
}

// =====================================================
// STOP IRRIGATION
// =====================================================

static void stopIrrigation(
    IrrigationState finalState,
    float currentTotalLitres
) {
    // Matikan relay terlebih dahulu demi keselamatan.
    relayOff();

    // -------------------------------------------------
    // CALCULATE SESSION USAGE
    // -------------------------------------------------

    lastSessionLitres =
        currentTotalLitres - irrigationStartLitres;

    if (lastSessionLitres < 0.0f) {
        lastSessionLitres = 0.0f;
    }

    lastSessionCost =
        lastSessionLitres * WATER_COST_PER_LITER;

    // -------------------------------------------------
    // STATE
    // -------------------------------------------------

    irrigationActive = false;
    irrigationState = finalState;
    scheduleHandledToday = true;

    // -------------------------------------------------
    // LOG
    // -------------------------------------------------

    Serial.println("[IRRIGATION] PUMP OFF");

    Serial.print("[IRRIGATION] Session usage: ");
    Serial.print(lastSessionLitres, 2);
    Serial.println(" L");

    Serial.print("[IRRIGATION] Session cost: ");
    Serial.print(lastSessionCost, 2);
    Serial.println();

    Serial.println("[IRRIGATION] ========================");
}

// =====================================================
// UPDATE
// =====================================================

void irrigationUpdate(
    const RTCData& rtc,
    const SoilData& soil,
    const RainData& rain,
    const FlowData& flow,
    const WaterLevelData& waterLevel
) {
    // =================================================
    // SAFETY: RTC
    // =================================================

    if (!rtc.valid) {
        if (irrigationActive) {
            stopIrrigation(
                IRRIGATION_SENSOR_ERROR,
                flow.totalLitres
            );
        } else {
            relayOff();
            irrigationState = IRRIGATION_SENSOR_ERROR;
        }

        return;
    }

    // =================================================
    // ACTIVE IRRIGATION
    // =================================================

    if (irrigationActive) {
        // ---------------------------------------------
        // SENSOR VALIDATION
        // ---------------------------------------------

        if (
            !soil.valid ||
            !rain.valid ||
            !flow.valid ||
            !waterLevel.valid
        ) {
            stopIrrigation(
                IRRIGATION_SENSOR_ERROR,
                flow.totalLitres
            );

            Serial.println(
                "[IRRIGATION] Sensor invalid -> PUMP OFF"
            );

            return;
        }

        // ---------------------------------------------
        // MAXIMUM RUNTIME
        // ---------------------------------------------

        const unsigned long runtime =
            millis() - irrigationStartMillis;

        if (runtime >= IRRIGATION_MAX_RUNTIME_MS) {
            stopIrrigation(
                IRRIGATION_TIMEOUT,
                flow.totalLitres
            );

            Serial.println(
                "[IRRIGATION] Maximum runtime reached"
            );

            return;
        }

        // ---------------------------------------------
        // FLOW DETECTION
        // ---------------------------------------------

        if (
            flow.flowRateLMin >=
            FLOW_MIN_DETECTED_LPM
        ) {
            lastFlowDetectionMillis = millis();
        }

        // ---------------------------------------------
        // NO FLOW PROTECTION
        // ---------------------------------------------

        if (
            millis() - lastFlowDetectionMillis >=
            FLOW_STARTUP_GRACE_MS
        ) {
            stopIrrigation(
                IRRIGATION_FLOW_ERROR,
                flow.totalLitres
            );

            Serial.println(
                "[IRRIGATION] No flow detected -> PUMP OFF"
            );

            return;
        }

        // ---------------------------------------------
        // LOW WATER
        // ---------------------------------------------

        if (
            waterLevel.waterLevelPercent <=
            WATER_LEVEL_MIN_IRRIGATION_PERCENT
        ) {
            stopIrrigation(
                IRRIGATION_LOW_WATER,
                flow.totalLitres
            );

            Serial.println(
                "[IRRIGATION] Water level low -> PUMP OFF"
            );

            return;
        }

        // ---------------------------------------------
        // SOIL TARGET
        // ---------------------------------------------

        // Soil masih di bawah target: pompa dapat berjalan.
        // Soil mencapai target: pompa dimatikan.

        if (
            soil.moisturePercent >=
            SOIL_IRRIGATION_STOP_PERCENT
        ) {
            stopIrrigation(
                IRRIGATION_COMPLETED,
                flow.totalLitres
            );

            Serial.print(
                "[IRRIGATION] Soil target reached: "
            );

            Serial.print(soil.moisturePercent, 1);
            Serial.println("%");

            return;
        }

        // Hujan ketika irigasi aktif tidak langsung mematikan
        // pompa. Penghentian tetap mengikuti target tanah
        // dan kondisi keselamatan lainnya.

        return;
    }

    // =================================================
    // PUMP MUST BE OFF
    // =================================================

    relayOff();

    // =================================================
    // SCHEDULE
    // =================================================

    if (!isWithinScheduleWindow(rtc)) {
        irrigationState = IRRIGATION_IDLE;
        return;
    }

    // =================================================
    // SOIL ALREADY WET
    // =================================================

    if (
        soil.valid &&
        soil.moisturePercent >=
        SOIL_IRRIGATION_STOP_PERCENT
    ) {
        scheduleHandledToday = true;
        irrigationState = IRRIGATION_SKIPPED;

        Serial.print(
            "[IRRIGATION] Soil already sufficient: "
        );

        Serial.print(soil.moisturePercent, 1);
        Serial.println("% -> SKIP");

        return;
    }

    // =================================================
    // RAIN DELAY
    // =================================================

    if (rain.valid && rain.raining) {
        irrigationState = IRRIGATION_RAIN_DELAY;

        Serial.println(
            "[IRRIGATION] Rain detected -> WAIT"
        );

        // Jadwal belum dianggap selesai.
        // Sistem akan mengecek lagi selama jendela jadwal
        // masih terbuka dan hujan sudah berhenti.

        return;
    }

    // =================================================
    // SENSOR VALIDATION
    // =================================================

    if (
        !soil.valid ||
        !flow.valid ||
        !waterLevel.valid
    ) {
        irrigationState = IRRIGATION_SENSOR_ERROR;
        relayOff();

        return;
    }

    // =================================================
    // WATER LEVEL CHECK
    // =================================================

    if (
        waterLevel.waterLevelPercent <=
        WATER_LEVEL_MIN_IRRIGATION_PERCENT
    ) {
        scheduleHandledToday = true;
        irrigationState = IRRIGATION_LOW_WATER;

        relayOff();

        Serial.print("[IRRIGATION] Water level: ");
        Serial.print(waterLevel.waterLevelPercent, 1);
        Serial.println("% -> LOW WATER");

        return;
    }

    // =================================================
    // START IRRIGATION
    // =================================================

    // Selama kelembapan tanah masih di bawah target,
    // penyiraman dapat dimulai.

    if (
        soil.moisturePercent <
        SOIL_IRRIGATION_STOP_PERCENT
    ) {
        startIrrigation(flow);
        return;
    }
}

// =====================================================
// ACTIVE STATE
// =====================================================

bool irrigationIsActive() {
    return irrigationActive;
}

// =====================================================
// GET STATE
// =====================================================

IrrigationState irrigationGetState() {
    return irrigationState;
}

// =====================================================
// SESSION LITRES
// =====================================================

float irrigationGetSessionLitres() {
    return lastSessionLitres;
}

// =====================================================
// SESSION COST
// =====================================================

float irrigationGetSessionCost() {
    return lastSessionCost;
}

// =====================================================
// EMERGENCY STOP
// =====================================================

void irrigationEmergencyStop() {
    relayOff();

    irrigationActive = false;
    irrigationState = IRRIGATION_TIMEOUT;
    scheduleHandledToday = true;

    Serial.println("[IRRIGATION] EMERGENCY STOP");
}