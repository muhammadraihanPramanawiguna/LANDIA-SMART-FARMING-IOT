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
// DHT SCHEDULER
// =====================================================

bool schedulerDHT() {

    unsigned long now = millis();

    if (now - lastDHT >= DHT_INTERVAL_MS) {

        lastDHT = now;

        return true;
    }

    return false;
}


// =====================================================
// SOIL SCHEDULER
// =====================================================

bool schedulerSoil() {

    unsigned long now = millis();

    if (now - lastSoil >= SOIL_INTERVAL_MS) {

        lastSoil = now;

        return true;
    }

    return false;
}


// =====================================================
// RAIN SCHEDULER
// =====================================================

bool schedulerRain() {

    unsigned long now = millis();

    if (now - lastRain >= RAIN_INTERVAL_MS) {

        lastRain = now;

        return true;
    }

    return false;
}


// =====================================================
// FLOW SCHEDULER
// =====================================================

bool schedulerFlow() {

    unsigned long now = millis();

    if (now - lastFlow >= FLOW_INTERVAL_MS) {

        lastFlow = now;

        return true;
    }

    return false;
}


// =====================================================
// WATER LEVEL / JSN SCHEDULER
// =====================================================

bool schedulerWaterLevel() {

    unsigned long now = millis();

    if (now - lastWaterLevel >= WATER_LEVEL_INTERVAL_MS) {

        lastWaterLevel = now;

        return true;
    }

    return false;
}


// =====================================================
// RTC SCHEDULER
// =====================================================

bool schedulerRTC() {

    unsigned long now = millis();

    if (now - lastRTC >= RTC_INTERVAL_MS) {

        lastRTC = now;

        return true;
    }

    return false;
}


// =====================================================
// LCD SCHEDULER
// =====================================================

bool schedulerLCD() {

    unsigned long now = millis();

    if (now - lastLCD >= LCD_INTERVAL_MS) {

        lastLCD = now;

        return true;
    }

    return false;
}


// =====================================================
// SERIAL MONITOR SCHEDULER
// =====================================================

bool schedulerSerial() {

    unsigned long now = millis();

    if (now - lastSerial >= SERIAL_INTERVAL_MS) {

        lastSerial = now;

        return true;
    }

    return false;
}