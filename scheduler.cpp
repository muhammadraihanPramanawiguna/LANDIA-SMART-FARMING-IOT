
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
// INTERNAL SCHEDULER HELPER
// =====================================================

// Memeriksa apakah interval sudah terpenuhi.
// Menggunakan unsigned subtraction agar aman terhadap
// overflow millis().
//
// Jika proses terlambat, jadwal berikutnya dihitung
// dari waktu eksekusi aktual. Tidak ada catch-up burst.

static bool schedulerIsDue(
    unsigned long &lastExecution,
    unsigned long interval,
    unsigned long currentTime
) {
    if (currentTime - lastExecution >= interval) {
        lastExecution = currentTime;
        return true;
    }

    return false;
}

// =====================================================
// INITIALIZATION
// =====================================================

void schedulerInit() {

    unsigned long now = millis();

    lastDHT = now;
    lastSoil = now;
    lastRain = now;
    lastFlow = now;
    lastWaterLevel = now;

    lastRTC = now;
    lastLCD = now;
    lastSerial = now;
}

// =====================================================
// SENSOR SCHEDULER
// =====================================================

bool schedulerDHT() {

    return schedulerIsDue(
        lastDHT,
        DHT_INTERVAL_MS,
        millis()
    );
}

bool schedulerSoil() {

    return schedulerIsDue(
        lastSoil,
        SOIL_INTERVAL_MS,
        millis()
    );
}

bool schedulerRain() {

    return schedulerIsDue(
        lastRain,
        RAIN_INTERVAL_MS,
        millis()
    );
}

bool schedulerFlow() {

    return schedulerIsDue(
        lastFlow,
        FLOW_INTERVAL_MS,
        millis()
    );
}

bool schedulerWaterLevel() {

    return schedulerIsDue(
        lastWaterLevel,
        WATER_LEVEL_INTERVAL_MS,
        millis()
    );
}

// =====================================================
// SYSTEM SCHEDULER
// =====================================================

bool schedulerRTC() {

    return schedulerIsDue(
        lastRTC,
        RTC_INTERVAL_MS,
        millis()
    );
}

bool schedulerLCD() {

    return schedulerIsDue(
        lastLCD,
        LCD_INTERVAL_MS,
        millis()
    );
}

bool schedulerSerial() {

    return schedulerIsDue(
        lastSerial,
        SERIAL_INTERVAL_MS,
        millis()
    );
}
