
#include <Arduino.h>
#include "scheduler.h"

// =====================================================
// LAST EXECUTION TIME
// =====================================================

static unsigned long lastDHT = 0;
static unsigned long lastSoil = 0;
static unsigned long lastRain = 0;
static unsigned long lastFlow = 0;
static unsigned long lastWaterLevel = 0;

static unsigned long lastRTC = 0;
static unsigned long lastLCD = 0;
static unsigned long lastSerial = 0;

// =====================================================
// SCHEDULER STATUS
// =====================================================

static bool schedulerInitialized = false;

// =====================================================
// INTERNAL SCHEDULER HELPER
// =====================================================

// Memeriksa apakah interval eksekusi telah terpenuhi.
//
// Menggunakan unsigned subtraction agar aman terhadap
// overflow millis().
//
// Jika proses terlambat:
// - Tidak melakukan catch-up burst.
// - Eksekusi berikutnya dihitung dari waktu aktual.
//
// Interval 0 tidak diperbolehkan.

static bool schedulerIsDue(
    unsigned long &lastExecution,
    unsigned long interval,
    unsigned long currentTime
) {

    // Proteksi interval tidak valid.
    if (interval == 0UL) {
        return false;
    }

    // Periksa apakah interval sudah terpenuhi.
    if (currentTime - lastExecution >= interval) {

        // Update waktu eksekusi aktual.
        lastExecution = currentTime;

        return true;
    }

    return false;
}

// =====================================================
// INITIALIZATION
// =====================================================

void schedulerInit() {

    const unsigned long now = millis();

    // -------------------------------------------------
    // SENSOR SCHEDULER
    // -------------------------------------------------

    lastDHT = now;
    lastSoil = now;
    lastRain = now;
    lastFlow = now;
    lastWaterLevel = now;

    // -------------------------------------------------
    // SYSTEM SCHEDULER
    // -------------------------------------------------

    lastRTC = now;
    lastLCD = now;
    lastSerial = now;

    // Scheduler baru boleh digunakan setelah init.
    schedulerInitialized = true;
}

// =====================================================
// SENSOR SCHEDULER
// =====================================================

bool schedulerDHT() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastDHT,
        DHT_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerSoil() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastSoil,
        SOIL_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerRain() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastRain,
        RAIN_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerFlow() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastFlow,
        FLOW_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerWaterLevel() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastWaterLevel,
        WATER_LEVEL_INTERVAL_MS,
        now
    );
}

// =====================================================
// SYSTEM SCHEDULER
// =====================================================

bool schedulerRTC() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastRTC,
        RTC_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerLCD() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastLCD,
        LCD_INTERVAL_MS,
        now
    );
}

// -----------------------------------------------------

bool schedulerSerial() {

    if (!schedulerInitialized) {
        return false;
    }

    const unsigned long now = millis();

    return schedulerIsDue(
        lastSerial,
        SERIAL_INTERVAL_MS,
        now
    );
}
