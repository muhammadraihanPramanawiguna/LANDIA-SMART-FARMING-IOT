
#include <Arduino.h>
#include <math.h>

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

static float lastKnownFlowTotalLitres = 0.0f;

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

    lastFlowDetectionMillis = 0;

    scheduleHandledToday = false;

    lastScheduleDateKey = -1;

    // SAFETY: PUMP MUST BE OFF DURING BOOT.

    relayOff();

    Serial.println("[IRRIGATION] Controller initialized");

    Serial.println("[IRRIGATION] Pump state: OFF");

    Serial.println("[IRRIGATION] Soil target: 85%");

    Serial.println("[IRRIGATION] Rain sensor does not block irrigation");
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

    const long correctedDateKey = getDateKey(rtc);

    const int correctedMinutes =
        rtc.hour * 60 + rtc.minute;

    const int scheduleMinutes =
        IRRIGATION_SCHEDULE_HOUR * 60 +
        IRRIGATION_SCHEDULE_MINUTE;

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
// SCHEDULE CHECK
// =====================================================

static bool isWithinScheduleWindow(const RTCData& rtc) {

    if (!rtc.valid) {
        return false;
    }

    const long currentDateKey = getDateKey(rtc);

    // -------------------------------------------------
    // NEW DAY
    // -------------------------------------------------

    if (currentDateKey != lastScheduleDateKey) {

        lastScheduleDateKey = currentDateKey;

        scheduleHandledToday = false;

        irrigationState = IRRIGATION_IDLE;
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
    // INSIDE SCHEDULE WINDOW
    // -------------------------------------------------

    if (currentMinutes <= deadlineMinutes) {

        return true;
    }

    // -------------------------------------------------
    // WINDOW EXPIRED
    // -------------------------------------------------

    scheduleHandledToday = true;

    irrigationState = IRRIGATION_SKIPPED;

    Serial.println(
        "[IRRIGATION] Schedule window expired"
    );

    return false;
}

// =====================================================
// START IRRIGATION
// =====================================================

static void startIrrigation(const FlowData& flow) {

    if (!isValidFlowData(flow)) {

        irrigationState = IRRIGATION_SENSOR_ERROR;

        relayOff();

        return;
    }

    irrigationStartLitres = getSafeFlowTotal(flow);

    lastSessionLitres = 0.0f;

    lastSessionCost = 0.0f;

    irrigationStartMillis = millis();

    lastFlowDetectionMillis = irrigationStartMillis;

    irrigationActive = true;

    irrigationState = IRRIGATION_IRRIGATING;

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
    const FlowData& flow
) {

    // SAFETY: TURN PUMP OFF FIRST.

    relayOff();

    // -------------------------------------------------
    // SESSION USAGE
    // -------------------------------------------------

    const float currentTotalLitres =
        getSafeFlowTotal(flow);

    lastSessionLitres =
        currentTotalLitres - irrigationStartLitres;

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
        lastSessionLitres * WATER_COST_PER_LITER;

    // -------------------------------------------------
    // UPDATE STATE
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

    Serial.println(lastSessionCost, 2);

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

    // Rain Sensor hanya untuk monitoring.
    // Tidak digunakan sebagai syarat atau penghalang irigasi.

    (void)rain;

    // =================================================
    // RTC SAFETY
    // =================================================

    if (!rtc.valid) {

        if (irrigationActive) {

            stopIrrigation(
                IRRIGATION_SENSOR_ERROR,
                flow
            );

        } else {

            relayOff();

            irrigationState = IRRIGATION_SENSOR_ERROR;
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
                flow
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
                flow
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
                flow
            );

            Serial.println(
                "[IRRIGATION] No flow detected -> PUMP OFF"
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
                flow
            );

            Serial.println(
                "[IRRIGATION] Water level low -> PUMP OFF"
            );

            return;
        }

        // ---------------------------------------------
        // SOIL TARGET
        // ---------------------------------------------

        if (
            soil.moisturePercent >=
            SOIL_IRRIGATION_STOP_PERCENT
        ) {

            stopIrrigation(
                IRRIGATION_COMPLETED,
                flow
            );

            Serial.print(
                "[IRRIGATION] Soil target reached: "
            );

            Serial.print(soil.moisturePercent, 1);

            Serial.println("%");

            return;
        }

        // Hujan tidak menghentikan penyiraman.
        // Pompa tetap berjalan sampai target soil
        // atau kondisi safety menghentikannya.

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

        irrigationState = IRRIGATION_SENSOR_ERROR;

        relayOff();

        Serial.println(
            "[IRRIGATION] Invalid control sensor -> START BLOCKED"
        );

        return;
    }

    // =================================================
    // SOIL ALREADY AT TARGET
    // =================================================

    if (
        soil.moisturePercent >=
        SOIL_IRRIGATION_START_PERCENT
    ) {

        scheduleHandledToday = true;

        irrigationState = IRRIGATION_SKIPPED;

        Serial.print(
            "[IRRIGATION] Soil already at target: "
        );

        Serial.print(soil.moisturePercent, 1);

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

    if (
        soil.moisturePercent <
        SOIL_IRRIGATION_START_PERCENT
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

    if (irrigationActive) {

        if (
            lastKnownFlowTotalLitres >=
            irrigationStartLitres
        ) {

            return
                lastKnownFlowTotalLitres -
                irrigationStartLitres;
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

        return
            irrigationGetSessionLitres() *
            WATER_COST_PER_LITER;
    }

    return lastSessionCost;
}

// =====================================================
// EMERGENCY STOP
// =====================================================

void irrigationEmergencyStop() {

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
            lastSessionLitres * WATER_COST_PER_LITER;
    }

    irrigationActive = false;

    irrigationState = IRRIGATION_TIMEOUT;

    scheduleHandledToday = true;

    Serial.println("[IRRIGATION] EMERGENCY STOP");
}
