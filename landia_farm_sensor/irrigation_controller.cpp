
#include <Arduino.h>
#include <math.h>

#include "config.h"
#include "irrigation_controller.h"
#include "relay_manager.h"
#include "mqtt_manager.h"

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
// LOW WATER LOG CONTROL
// =====================================================

static bool lowWaterLogged = false;

// =====================================================
// SENSOR VALIDATION
// =====================================================

static bool isValidPercentage(float value) {
    return isfinite(value) &&
           value >= 0.0f &&
           value <= 100.0f;
}

static bool isValidFlowData(const FlowData& flow) {
    return flow.valid &&
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
    return soil.valid &&
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
    return (rtc.year * 10000L) +
           (rtc.month * 100L) +
           rtc.day;
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

    lastFlowDetectionMillis = 0;

    scheduleHandledToday = false;
    lastScheduleDateKey = -1;

    lowWaterLogged = false;

    // SAFETY: PUMP MUST BE OFF DURING BOOT.
    relayOff();

    Serial.println("[IRRIGATION] Controller initialized");
    Serial.println("[IRRIGATION] Pump state: OFF");
    Serial.println("[IRRIGATION] Soil target: 85%");
    Serial.println("[IRRIGATION] Automatic irrigation enabled");
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

    const bool sameDate =
        correctedDateKey == lastScheduleDateKey;

    // Jika tanggal berubah, buka jadwal untuk tanggal baru.
    // Jika tanggal sama, pertahankan status jadwal sebelumnya.
    if (!sameDate) {
        lastScheduleDateKey = correctedDateKey;
        scheduleHandledToday = false;
        irrigationState = IRRIGATION_IDLE;
    }

    Serial.println("[IRRIGATION] RTC correction detected");

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

        lowWaterLogged = false;

        irrigationState = IRRIGATION_IDLE;

        Serial.println(
            "[IRRIGATION] New day - Schedule reset"
        );
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

    irrigationStartLitres = getSafeFlowTotal(flow);

    lastSessionLitres = 0.0f;
    lastSessionCost = 0.0f;

    irrigationStartMillis = millis();

    lastFlowDetectionMillis = irrigationStartMillis;

    irrigationActive = true;

    irrigationState = IRRIGATION_IRRIGATING;

    lowWaterLogged = false;

    relayOn();

    Serial.println();
    Serial.println("[IRRIGATION] ========================");
    Serial.println("[IRRIGATION] PUMP ON");

    Serial.print("[IRRIGATION] Start total: ");
    Serial.print(irrigationStartLitres, 2);
    Serial.println(" L");

    // Simpan event STARTED ke antrean LittleFS.
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

    // LOW WATER boleh dicoba kembali selama
    // jendela jadwal masih berlaku.
    //
    // Kondisi lainnya dianggap menyelesaikan
    // atau menggagalkan jadwal hari ini.

    if (finalState != IRRIGATION_LOW_WATER) {
        scheduleHandledToday = true;
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

    Serial.print("[IRRIGATION] Session cost: ");
    Serial.println(lastSessionCost, 2);

    Serial.print("[IRRIGATION] Final state: ");
    Serial.println(getIrrigationEventState(finalState));

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
                flow,
                rtc
            );

            Serial.println(
                "[IRRIGATION] No flow detected -> PUMP OFF"
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
                flow,
                rtc
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

        if (
            irrigationState !=
            IRRIGATION_SENSOR_ERROR
        ) {
            Serial.println(
                "[IRRIGATION] Invalid sensor -> START BLOCKED"
            );
        }

        irrigationState = IRRIGATION_SENSOR_ERROR;

        relayOff();

        // Tidak mengunci jadwal.
        // Sensor dapat pulih sebelum window berakhir.

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

        irrigationState = IRRIGATION_LOW_WATER;

        relayOff();

        // Log hanya ketika pertama kali masuk
        // kondisi LOW WATER.

        if (!lowWaterLogged) {

            Serial.print("[IRRIGATION] Water level: ");

            Serial.print(
                waterLevel.waterLevelPercent,
                1
            );

            Serial.println(
                "% -> LOW WATER, WAITING FOR REFILL"
            );

            lowWaterLogged = true;
        }

        // PENTING:
        // scheduleHandledToday tetap FALSE.
        // ESP32 dapat mencoba kembali secara otomatis.

        return;
    }

    // =================================================
    // WATER RECOVERED
    // =================================================

    if (lowWaterLogged) {

        Serial.print("[IRRIGATION] Water recovered: ");

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
        SOIL_IRRIGATION_START_PERCENT
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
            lastSessionLitres *
            WATER_COST_PER_LITER;
    }

    irrigationActive = false;

    irrigationState = IRRIGATION_TIMEOUT;

    scheduleHandledToday = true;

    Serial.println("[IRRIGATION] EMERGENCY STOP");
}
