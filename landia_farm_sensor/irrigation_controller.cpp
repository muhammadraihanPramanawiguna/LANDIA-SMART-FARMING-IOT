
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "config.h"
#include "irrigation_controller.h"
#include "relay_manager.h"
#include "mqtt_manager.h"

// =====================================================
// NVS CONFIGURATION
// =====================================================

static const char* IRRIGATION_NVS_NAMESPACE = "landia_irrig";

static const char* KEY_DAILY_DATE = "daily_date";
static const char* KEY_DAILY_LITRES = "daily_litres";
static const char* KEY_LAST_SESSION_L = "session_l";
static const char* KEY_LAST_SESSION_COST = "session_cost";
static const char* KEY_SESSION_ACTIVE = "session_active";
static const char* KEY_SESSION_START_L = "session_start";
static const char* KEY_SCHEDULE_HANDLED_DATE = "sched_done_date";

static Preferences irrigationPreferences;

static bool irrigationStorageReady = false;

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
static float lastKnownFlowTotalLitres = 0.0f;

// =====================================================
// DAILY STATISTICS
// =====================================================

static float dailyIrrigationLitres = 0.0f;
static float dailyIrrigationCost = 0.0f;

static long dailyDateKey = -1;

static float lastDailyPersistLitres = 0.0f;
static unsigned long lastDailyPersistMillis = 0;

static float lastObservedFlowTotalLitres = 0.0f;
static bool flowBaselineInitialized = false;

// =====================================================
// FLOW MONITOR
// =====================================================

static unsigned long lastFlowDetectionMillis = 0;

// true sampai flow pertama berhasil terdeteksi.
static bool flowStartupPending = false;

// Waktu awal fase startup.
static unsigned long flowStartupStartedMillis = 0;

// Menghindari log startup berulang setiap update.
static bool flowStartupLogPrinted = false;

// =====================================================
// DAILY SCHEDULE
// =====================================================

static bool scheduleHandledToday = false;
static long lastScheduleDateKey = -1;
static bool previousSessionInterrupted = false;

// =====================================================
// LOW WATER LOG CONTROL
// =====================================================

static bool lowWaterLogged = false;

// =====================================================
// SENSOR VALIDATION
// =====================================================

static bool isValidPercentage(float value) {

    return
        isfinite(value) &&
        value >= 0.0f &&
        value <= 100.0f;
}

static bool isValidFlowData(const FlowData& flow) {

    return
        flow.valid &&
        isfinite(flow.flowRateLMin) &&
        isfinite(flow.totalLitres) &&
        flow.flowRateLMin >= 0.0f &&
        flow.totalLitres >= 0.0f;
}

static bool isValidControlSensors(
    const SoilData& soil,
    const FlowData& flow,
    const WaterLevelData& waterLevel
) {

    return
        soil.valid &&
        isValidPercentage(soil.moisturePercent) &&
        isValidFlowData(flow) &&
        waterLevel.valid &&
        isValidPercentage(waterLevel.waterLevelPercent);
}

// =====================================================
// TARIFF AND COST
// =====================================================

static float calculateWaterCost(float litres) {

    if (
        !isfinite(litres) ||
        litres < 0.0f ||
        !isfinite(WATER_TARIFF_PER_M3) ||
        WATER_TARIFF_PER_M3 < 0.0f ||
        !isfinite(LITERS_PER_CUBIC_METER) ||
        LITERS_PER_CUBIC_METER <= 0.0f
    ) {
        return 0.0f;
    }

    const float cubicMeter =
        litres / LITERS_PER_CUBIC_METER;

    return cubicMeter * WATER_TARIFF_PER_M3;
}

// =====================================================
// FLOW TOTAL
// =====================================================

static float getSafeFlowTotal(const FlowData& flow) {

    if (isValidFlowData(flow)) {
        lastKnownFlowTotalLitres = flow.totalLitres;
    }

    return lastKnownFlowTotalLitres;
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

// Simpan tanggal jadwal yang sudah selesai/di-skip agar reboot
// tidak memicu penyiraman kedua pada hari yang sama.
static void persistScheduleHandledDate(long dateKey) {
    if (irrigationStorageReady) {
        irrigationPreferences.putLong(
            KEY_SCHEDULE_HANDLED_DATE,
            dateKey
        );
    }
}

// =====================================================
// NVS SAVE DAILY STATISTICS
// =====================================================

static bool persistDailyStatistics(bool force) {

    if (!irrigationStorageReady || dailyDateKey < 0) {
        return false;
    }

    const unsigned long now = millis();

    const bool timeReached =
        (now - lastDailyPersistMillis) >=
        FLOW_PERSIST_INTERVAL_MS;

    const float volumeDifference =
        fabsf(
            dailyIrrigationLitres -
            lastDailyPersistLitres
        );

    const bool volumeReached =
        volumeDifference >=
        FLOW_PERSIST_VOLUME_THRESHOLD_L;

    if (!force && !timeReached && !volumeReached) {
        return true;
    }

    bool success = true;

    if (
        irrigationPreferences.putLong(
            KEY_DAILY_DATE,
            dailyDateKey
        ) == 0
    ) {
        success = false;
    }

    if (
        irrigationPreferences.putFloat(
            KEY_DAILY_LITRES,
            dailyIrrigationLitres
        ) == 0
    ) {
        success = false;
    }

    if (success) {

        lastDailyPersistLitres =
            dailyIrrigationLitres;

        lastDailyPersistMillis = now;

    } else {

        Serial.println(
            "[IRRIGATION] ERROR: Daily NVS save failed"
        );

    }

    return success;
}

// =====================================================
// NVS SAVE LAST SESSION
// =====================================================

static bool persistLastSession() {

    if (!irrigationStorageReady) {
        return false;
    }

    const size_t litresWritten =
        irrigationPreferences.putFloat(
            KEY_LAST_SESSION_L,
            lastSessionLitres
        );

    const size_t costWritten =
        irrigationPreferences.putFloat(
            KEY_LAST_SESSION_COST,
            lastSessionCost
        );

    return
        litresWritten > 0 &&
        costWritten > 0;
}

// =====================================================
// DAILY STATISTICS UPDATE
// =====================================================

static void updateDailyStatistics(
    const RTCData& rtc,
    const FlowData& flow
) {

    if (!rtc.valid || !isValidFlowData(flow)) {
        return;
    }

    const long currentDateKey = getDateKey(rtc);

    // -------------------------------------------------
    // NEW DAY
    // -------------------------------------------------

    if (currentDateKey != dailyDateKey) {

        dailyDateKey = currentDateKey;

        dailyIrrigationLitres = 0.0f;
        dailyIrrigationCost = 0.0f;

        lastDailyPersistLitres = 0.0f;
        lastDailyPersistMillis = millis();

        if (irrigationStorageReady) {

            irrigationPreferences.putLong(
                KEY_DAILY_DATE,
                dailyDateKey
            );

            irrigationPreferences.putFloat(
                KEY_DAILY_LITRES,
                0.0f
            );

        }

        Serial.println(
            "[IRRIGATION] Daily statistics reset for new day"
        );
    }

    // -------------------------------------------------
    // INITIAL FLOW BASELINE
    // -------------------------------------------------

    if (!flowBaselineInitialized) {

        lastObservedFlowTotalLitres =
            flow.totalLitres;

        flowBaselineInitialized = true;

        return;
    }

    // -------------------------------------------------
    // FLOW DELTA
    // -------------------------------------------------

    const float currentTotal =
        flow.totalLitres;

    float deltaLitres =
        currentTotal - lastObservedFlowTotalLitres;

    // Flow total may only increase.
    // A decrease indicates a reset or inconsistent data.
    if (
        !isfinite(deltaLitres) ||
        deltaLitres < 0.0f
    ) {

        lastObservedFlowTotalLitres =
            currentTotal;

        return;
    }

    lastObservedFlowTotalLitres =
        currentTotal;

    // -------------------------------------------------
    // COUNT ONLY WHILE IRRIGATION IS ACTIVE
    // -------------------------------------------------

    if (irrigationActive && deltaLitres > 0.0f) {

        dailyIrrigationLitres += deltaLitres;

        dailyIrrigationCost =
            calculateWaterCost(
                dailyIrrigationLitres
            );

    }

    // -------------------------------------------------
    // DAILY CHECKPOINT
    // -------------------------------------------------

    persistDailyStatistics(false);
}

// =====================================================
// EVENT STATE
// =====================================================

static const char* getIrrigationEventState(
    IrrigationState state
) {

    switch (state) {

        case IRRIGATION_COMPLETED:
            return "COMPLETED";

        case IRRIGATION_LOW_WATER:
            return "LOW_WATER";

        case IRRIGATION_SENSOR_ERROR:
            return "SENSOR_ERROR";

        case IRRIGATION_FLOW_ERROR:
            return "FLOW_ERROR";

        case IRRIGATION_TIMEOUT:
            return "TIMEOUT";

        case IRRIGATION_SKIPPED:
            return "SKIPPED";

        default:
            return "STOPPED";
    }
}

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
    lastKnownFlowTotalLitres = 0.0f;

    dailyIrrigationLitres = 0.0f;
    dailyIrrigationCost = 0.0f;

    dailyDateKey = -1;

    lastDailyPersistLitres = 0.0f;
    lastDailyPersistMillis = millis();

    lastObservedFlowTotalLitres = 0.0f;
    flowBaselineInitialized = false;

    lastFlowDetectionMillis = 0;

flowStartupPending = false;
flowStartupStartedMillis = 0;
flowStartupLogPrinted = false;

    scheduleHandledToday = false;
    lastScheduleDateKey = -1;
    previousSessionInterrupted = false;

    lowWaterLogged = false;

    // -------------------------------------------------
    // SAFETY: PUMP OFF DURING BOOT
    // -------------------------------------------------

    relayOff();

    // -------------------------------------------------
    // INITIALIZE NVS
    // -------------------------------------------------

    irrigationStorageReady =
        irrigationPreferences.begin(
            IRRIGATION_NVS_NAMESPACE,
            false
        );

    if (irrigationStorageReady) {

        lastSessionLitres =
            irrigationPreferences.getFloat(
                KEY_LAST_SESSION_L,
                0.0f
            );

        lastSessionCost =
            irrigationPreferences.getFloat(
                KEY_LAST_SESSION_COST,
                0.0f
            );

        // A previous active session means ESP32
        // restarted before the session completed.
        const bool previousSessionActive =
            irrigationPreferences.getBool(
                KEY_SESSION_ACTIVE,
                false
            );

        if (previousSessionActive) {

            previousSessionInterrupted = true;

            const float previousStartLitres =
                irrigationPreferences.getFloat(
                    KEY_SESSION_START_L,
                    0.0f
                );

            // Flow meter total is restored separately
            // by flowInit(). Its current value is not
            // available yet in irrigationInit().
            // Therefore, do not fabricate session usage.

            lastSessionLitres = 0.0f;
            lastSessionCost = 0.0f;

            irrigationPreferences.putFloat(
                KEY_LAST_SESSION_L,
                0.0f
            );

            irrigationPreferences.putFloat(
                KEY_LAST_SESSION_COST,
                0.0f
            );

            irrigationPreferences.putBool(
                KEY_SESSION_ACTIVE,
                false
            );

            (void)previousStartLitres;

            Serial.println(
                "[IRRIGATION] Previous session interrupted by restart"
            );

        }

        dailyDateKey =
            irrigationPreferences.getLong(
                KEY_DAILY_DATE,
                -1
            );

        dailyIrrigationLitres =
            irrigationPreferences.getFloat(
                KEY_DAILY_LITRES,
                0.0f
            );

        if (
            !isfinite(dailyIrrigationLitres) ||
            dailyIrrigationLitres < 0.0f
        ) {
            dailyIrrigationLitres = 0.0f;
        }

        dailyIrrigationCost =
            calculateWaterCost(
                dailyIrrigationLitres
            );

        // Status schedule dipulihkan setelah tanggal RTC diketahui
        // di irrigationUpdate(), bukan langsung di sini.
        lastDailyPersistLitres =
            dailyIrrigationLitres;

        Serial.println(
            "[IRRIGATION] NVS statistics restored"
        );

    } else {

        Serial.println(
            "[IRRIGATION] WARNING: NVS initialization failed"
        );

    }

    Serial.println(
        "[IRRIGATION] Controller initialized"
    );

    Serial.println(
        "[IRRIGATION] Pump state: OFF"
    );

    Serial.println(
        "[IRRIGATION] Soil target: 85%"
    );

    Serial.println(
        "[IRRIGATION] Automatic irrigation enabled"
    );

    Serial.println(
        "[IRRIGATION] Rain sensor does not block irrigation"
    );
}

// =====================================================
// RTC CORRECTION SAFETY
// =====================================================

void irrigationNotifyTimeCorrection(
    const RTCData& rtc
) {

    if (!rtc.valid) {

        Serial.println(
            "[IRRIGATION] RTC correction ignored: invalid RTC data"
        );

        return;
    }

    const long correctedDateKey =
        getDateKey(rtc);

    const bool sameDate =
        correctedDateKey == lastScheduleDateKey;

    if (!sameDate) {

        lastScheduleDateKey =
            correctedDateKey;

        const long persistedHandledDate =
            irrigationStorageReady
                ? irrigationPreferences.getLong(
                    KEY_SCHEDULE_HANDLED_DATE,
                    -1
                )
                : -1;

        scheduleHandledToday =
            (persistedHandledDate == correctedDateKey);

        if (previousSessionInterrupted) {
            scheduleHandledToday = true;
            persistScheduleHandledDate(correctedDateKey);
            previousSessionInterrupted = false;
        }

        // Jangan mengubah state sesi yang sedang berjalan.
        // Koreksi tanggal hanya mereset status jadwal.
        if (!irrigationActive) {
            irrigationState = IRRIGATION_IDLE;
        }

    }

    Serial.println(
        "[IRRIGATION] RTC correction detected"
    );

    Serial.printf(
        "[IRRIGATION] Corrected RTC: %04d-%02d-%02d %02d:%02d\n",
        rtc.year,
        rtc.month,
        rtc.day,
        rtc.hour,
        rtc.minute
    );

    if (scheduleHandledToday) {

        Serial.println(
            "[IRRIGATION] Today's schedule already handled"
        );

    } else {

        Serial.println(
            "[IRRIGATION] Schedule remains available"
        );

    }
}

// =====================================================
// SCHEDULE CHECK
// =====================================================

static bool isWithinScheduleWindow(
    const RTCData& rtc
) {

    if (!rtc.valid) {
        return false;
    }

    const long currentDateKey =
        getDateKey(rtc);

    if (currentDateKey != lastScheduleDateKey) {

        lastScheduleDateKey =
            currentDateKey;

        const long persistedHandledDate =
            irrigationStorageReady
                ? irrigationPreferences.getLong(
                    KEY_SCHEDULE_HANDLED_DATE,
                    -1
                )
                : -1;

        scheduleHandledToday =
            (persistedHandledDate == currentDateKey);

        // Jika reboot memutus sesi pompa, jangan otomatis mulai
        // sesi kedua pada hari itu: kondisi tanah/air saat restart
        // belum tentu sama dengan kondisi sebelum reboot.
        if (previousSessionInterrupted) {
            scheduleHandledToday = true;
            persistScheduleHandledDate(currentDateKey);
            previousSessionInterrupted = false;
        }

        lowWaterLogged = false;

        if (!irrigationActive) {
            irrigationState = scheduleHandledToday
                ? IRRIGATION_SKIPPED
                : IRRIGATION_IDLE;
        }

        Serial.println(
            "[IRRIGATION] New day - Schedule reset"
        );

    }

    const int currentMinutes =
        rtc.hour * 60 + rtc.minute;

    const int scheduleMinutes =
        IRRIGATION_SCHEDULE_HOUR * 60 +
        IRRIGATION_SCHEDULE_MINUTE;

    const int deadlineMinutes =
        scheduleMinutes +
        IRRIGATION_MAX_DELAY_MINUTES;

    if (currentMinutes < scheduleMinutes) {
        return false;
    }

    if (scheduleHandledToday) {
        return false;
    }

    if (currentMinutes <= deadlineMinutes) {
        return true;
    }

    scheduleHandledToday = true;
    persistScheduleHandledDate(currentDateKey);

    irrigationState = IRRIGATION_SKIPPED;

    Serial.println(
        "[IRRIGATION] Schedule window expired"
    );

    return false;
}

// =====================================================
// START IRRIGATION
// =====================================================

static void startIrrigation(
    const FlowData& flow,
    const RTCData& rtc
) {

    if (!isValidFlowData(flow)) {

        irrigationState = IRRIGATION_SENSOR_ERROR;

        relayOff();

        Serial.println(
            "[IRRIGATION] Invalid flow data - START BLOCKED"
        );

        return;
    }

    irrigationStartLitres =
        getSafeFlowTotal(flow);

    lastSessionLitres = 0.0f;
    lastSessionCost = 0.0f;

irrigationStartMillis = millis();

lastFlowDetectionMillis =
    irrigationStartMillis;

// Mulai fase startup flow detection.
flowStartupPending = true;

flowStartupStartedMillis =
    irrigationStartMillis;

flowStartupLogPrinted = false;

irrigationActive = true;

    irrigationState = IRRIGATION_IRRIGATING;

    lowWaterLogged = false;

    // Save active session marker before activating pump.
    if (irrigationStorageReady) {

        irrigationPreferences.putFloat(
            KEY_SESSION_START_L,
            irrigationStartLitres
        );

        irrigationPreferences.putBool(
            KEY_SESSION_ACTIVE,
            true
        );

    }

    relayOn();

    Serial.println();
    Serial.println("[IRRIGATION] ========================");
    Serial.println("[IRRIGATION] PUMP ON");

    Serial.print("[IRRIGATION] Start total: ");
    Serial.print(irrigationStartLitres, 2);
    Serial.println(" L");

    mqttPublishIrrigationEvent(
        "IRRIGATION",
        "STARTED",
        rtc
    );
}

// =====================================================
// STOP IRRIGATION
// =====================================================

static void stopIrrigation(
    IrrigationState finalState,
    const FlowData& flow,
    const RTCData& rtc
) {

    // SAFETY: PUMP OFF FIRST.
    relayOff();

    // -------------------------------------------------
    // SESSION USAGE
    // -------------------------------------------------

    const float currentTotal =
        getSafeFlowTotal(flow);

    lastSessionLitres =
        currentTotal - irrigationStartLitres;

    if (
        !isfinite(lastSessionLitres) ||
        lastSessionLitres < 0.0f
    ) {
        lastSessionLitres = 0.0f;
    }

    // -------------------------------------------------
    // SESSION COST
    // -------------------------------------------------

    lastSessionCost =
        calculateWaterCost(lastSessionLitres);

    // -------------------------------------------------
    // UPDATE DAILY STATISTICS
    // -------------------------------------------------

    dailyIrrigationCost =
        calculateWaterCost(dailyIrrigationLitres);

    // -------------------------------------------------
    // UPDATE STATE
    // -------------------------------------------------

    irrigationActive = false;

    flowStartupPending = false;
    flowStartupStartedMillis = 0;
    flowStartupLogPrinted = false;

    irrigationState = finalState;

    if (finalState != IRRIGATION_LOW_WATER) {
        scheduleHandledToday = true;
        if (rtc.valid) {
            persistScheduleHandledDate(getDateKey(rtc));
        }
    }

    // -------------------------------------------------
    // SAVE SESSION AND DAILY STATISTICS
    // -------------------------------------------------

    if (irrigationStorageReady) {

        persistLastSession();

        persistDailyStatistics(true);

        irrigationPreferences.putBool(
            KEY_SESSION_ACTIVE,
            false
        );

    }

    // -------------------------------------------------
    // EVENT STORAGE
    // -------------------------------------------------

    mqttPublishIrrigationEvent(
        "IRRIGATION",
        getIrrigationEventState(finalState),
        rtc
    );

    // -------------------------------------------------
    // LOG
    // -------------------------------------------------

    Serial.println("[IRRIGATION] PUMP OFF");

    Serial.print("[IRRIGATION] Session usage: ");
    Serial.print(lastSessionLitres, 2);
    Serial.println(" L");

    Serial.print("[IRRIGATION] Session cost: Rp ");
    Serial.println(lastSessionCost, 2);

    Serial.print("[IRRIGATION] Daily usage: ");
    Serial.print(dailyIrrigationLitres, 2);
    Serial.println(" L");

    Serial.print("[IRRIGATION] Daily cost: Rp ");
    Serial.println(dailyIrrigationCost, 2);

    Serial.print("[IRRIGATION] Final state: ");
    Serial.println(
        getIrrigationEventState(finalState)
    );

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

    // Rain sensor hanya untuk monitoring.
    // Tidak digunakan sebagai penghalang irigasi.
    (void)rain;

    // =================================================
    // RTC SAFETY
    // =================================================

    if (!rtc.valid) {

        if (irrigationActive) {

            stopIrrigation(
                IRRIGATION_SENSOR_ERROR,
                flow,
                rtc
            );

        } else {

            relayOff();

            irrigationState =
                IRRIGATION_SENSOR_ERROR;

        }

        return;
    }

    // =================================================
    // UPDATE LAST KNOWN FLOW
    // =================================================

    if (isValidFlowData(flow)) {
        getSafeFlowTotal(flow);
    }

    // =================================================
    // DAILY STATISTICS
    // =================================================

    updateDailyStatistics(rtc, flow);

    // =================================================
    // ACTIVE IRRIGATION
    // =================================================

    if (irrigationActive) {

        // ---------------------------------------------
        // SENSOR VALIDATION
        // ---------------------------------------------

        if (
            !isValidControlSensors(
                soil,
                flow,
                waterLevel
            )
        ) {

            stopIrrigation(
                IRRIGATION_SENSOR_ERROR,
                flow,
                rtc
            );

            Serial.println(
                "[IRRIGATION] Invalid control sensor -> PUMP OFF"
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
                flow,
                rtc
            );

            Serial.println(
                "[IRRIGATION] Maximum runtime reached"
            );

            return;
        }

        // ---------------------------------------------
        // LOW WATER PROTECTION
        // ---------------------------------------------

        if (
            waterLevel.waterLevelPercent <=
            WATER_LEVEL_MIN_IRRIGATION_PERCENT
        ) {

            stopIrrigation(
                IRRIGATION_LOW_WATER,
                flow,
                rtc
            );

            Serial.println(
                "[IRRIGATION] Low water -> PUMP OFF"
            );

            lowWaterLogged = true;

            return;
        }

        // ---------------------------------------------
        // FLOW DETECTION + STARTUP / FLOW-LOSS PROTECTION
        // ---------------------------------------------

        const unsigned long now = millis();

        const bool flowDetected =
            flow.flowRateLMin >= FLOW_MIN_DETECTED_LPM;

        // PHASE A: STARTUP FLOW DETECTION
        // Pompa baru aktif; tunggu flow pertama.
        if (flowStartupPending) {

            if (flowDetected) {

                flowStartupPending = false;
                lastFlowDetectionMillis = now;

                Serial.print("[IRRIGATION] Flow detected: ");
                Serial.print(flow.flowRateLMin, 2);
                Serial.println(" L/min -> normal flow monitoring enabled");

            } else {

                if (!flowStartupLogPrinted) {
                    Serial.println(
                        "[IRRIGATION] Startup phase: waiting for flow pulses"
                    );
                    flowStartupLogPrinted = true;
                }

                if (
                    now - flowStartupStartedMillis >=
                    FLOW_STARTUP_DETECTION_TIMEOUT_MS
                ) {

                    stopIrrigation(
                        IRRIGATION_FLOW_ERROR,
                        flow,
                        rtc
                    );

                    Serial.println(
                        "[IRRIGATION] Startup flow timeout -> PUMP OFF"
                    );

                    return;
                }
            }
        }

        // PHASE B: NORMAL FLOW MONITORING
        // Flow pernah terdeteksi; jika hilang, matikan pompa.
        else {

            if (flowDetected) {
                lastFlowDetectionMillis = now;
            }

            if (
                now - lastFlowDetectionMillis >=
                FLOW_LOSS_TIMEOUT_MS
            ) {

                stopIrrigation(
                    IRRIGATION_FLOW_ERROR,
                    flow,
                    rtc
                );

                Serial.println(
                    "[IRRIGATION] Flow lost -> PUMP OFF"
                );

                return;
            }
        }

        // ---------------------------------------------
        // SOIL TARGET
        // ---------------------------------------------

        
        // ---------------------------------------------

        if (
            soil.moisturePercent >=
            SOIL_IRRIGATION_STOP_PERCENT
        ) {

            stopIrrigation(
                IRRIGATION_COMPLETED,
                flow,
                rtc
            );

            Serial.print(
                "[IRRIGATION] Soil target reached: "
            );

            Serial.print(
                soil.moisturePercent,
                1
            );

            Serial.println("%");

            return;
        }

        // Hujan tidak menghentikan penyiraman.
        return;
    }

    // =================================================
    // PUMP MUST BE OFF
    // =================================================

    relayOff();

    // =================================================
    // SCHEDULE CHECK
    // =================================================

    if (!isWithinScheduleWindow(rtc)) {
        return;
    }

    // =================================================
    // CONTROL SENSOR VALIDATION
    // =================================================

    if (
        !isValidControlSensors(
            soil,
            flow,
            waterLevel
        )
    ) {

        if (
            irrigationState !=
            IRRIGATION_SENSOR_ERROR
        ) {

            Serial.println(
                "[IRRIGATION] Invalid sensor -> START BLOCKED"
            );

        }

        irrigationState =
            IRRIGATION_SENSOR_ERROR;

        relayOff();

        return;
    }

    // =================================================
    // SOIL ALREADY AT TARGET
    // =================================================

    if (
        soil.moisturePercent >=
        SOIL_IRRIGATION_STOP_PERCENT
    ) {

        scheduleHandledToday = true;
        persistScheduleHandledDate(getDateKey(rtc));

        irrigationState =
            IRRIGATION_SKIPPED;

        Serial.print(
            "[IRRIGATION] Soil already at target: "
        );

        Serial.print(
            soil.moisturePercent,
            1
        );

        Serial.println("% -> SKIP");

        return;
    }

    // =================================================
    // WATER LEVEL CHECK
    // =================================================

    if (
        waterLevel.waterLevelPercent <=
        WATER_LEVEL_MIN_IRRIGATION_PERCENT
    ) {

        irrigationState =
            IRRIGATION_LOW_WATER;

        relayOff();

        if (!lowWaterLogged) {

            Serial.print(
                "[IRRIGATION] Water level: "
            );

            Serial.print(
                waterLevel.waterLevelPercent,
                1
            );

            Serial.println(
                "% -> LOW WATER, WAITING FOR REFILL"
            );

            lowWaterLogged = true;
        }

        // Jadwal tetap dapat dicoba kembali
        // selama jendela jadwal belum berakhir.
        return;
    }

    // =================================================
    // WATER RECOVERED
    // =================================================

    if (lowWaterLogged) {

        Serial.print(
            "[IRRIGATION] Water recovered: "
        );

        Serial.print(
            waterLevel.waterLevelPercent,
            1
        );

        Serial.println("%");

        lowWaterLogged = false;
    }

    // =================================================
    // START IRRIGATION
    // =================================================

    if (
        soil.moisturePercent <
        SOIL_IRRIGATION_STOP_PERCENT
    ) {

        startIrrigation(
            flow,
            rtc
        );

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

    if (irrigationActive) {

        const float currentTotal =
            lastKnownFlowTotalLitres;

        const float sessionLitres =
            currentTotal - irrigationStartLitres;

        if (
            isfinite(sessionLitres) &&
            sessionLitres >= 0.0f
        ) {
            return sessionLitres;
        }

        return 0.0f;
    }

    return lastSessionLitres;
}

// =====================================================
// SESSION COST
// =====================================================

float irrigationGetSessionCost() {

    if (irrigationActive) {

        return calculateWaterCost(
            irrigationGetSessionLitres()
        );
    }

    return lastSessionCost;
}

// =====================================================
// DAILY LITRES
// =====================================================

float irrigationGetDailyLitres() {
    return dailyIrrigationLitres;
}

// =====================================================
// DAILY COST
// =====================================================

float irrigationGetDailyCost() {

    return calculateWaterCost(
        dailyIrrigationLitres
    );
}

// =====================================================
// EMERGENCY STOP
// =====================================================

void irrigationEmergencyStop() {

    // SAFETY FIRST.
    relayOff();

    if (irrigationActive) {

        lastSessionLitres =
            lastKnownFlowTotalLitres -
            irrigationStartLitres;

        if (
            !isfinite(lastSessionLitres) ||
            lastSessionLitres < 0.0f
        ) {
            lastSessionLitres = 0.0f;
        }

        lastSessionCost =
            calculateWaterCost(
                lastSessionLitres
            );

        irrigationActive = false;

        flowStartupPending = false;
        flowStartupStartedMillis = 0;
        flowStartupLogPrinted = false;

        irrigationState =
            IRRIGATION_TIMEOUT;

        scheduleHandledToday = true;
        if (lastScheduleDateKey >= 0) {
            persistScheduleHandledDate(lastScheduleDateKey);
        }

        if (irrigationStorageReady) {

            persistLastSession();

            persistDailyStatistics(true);

            irrigationPreferences.putBool(
                KEY_SESSION_ACTIVE,
                false
            );

        }
    }

    Serial.println(
        "[IRRIGATION] EMERGENCY STOP"
    );
}
