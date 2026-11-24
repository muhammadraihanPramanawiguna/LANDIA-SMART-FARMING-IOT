
#ifndef IRRIGATION_CONTROLLER_H
#define IRRIGATION_CONTROLLER_H

#include <Arduino.h>

#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"
#include "rtc_manager.h"

// =====================================================
// IRRIGATION STATE
// =====================================================

enum IrrigationState {
    IRRIGATION_IDLE,
    IRRIGATION_WAITING,
    IRRIGATION_IRRIGATING,
    IRRIGATION_COMPLETED,
    IRRIGATION_SKIPPED,
    IRRIGATION_LOW_WATER,
    IRRIGATION_RAIN_DELAY,
    IRRIGATION_SENSOR_ERROR,
    IRRIGATION_FLOW_ERROR,
    IRRIGATION_TIMEOUT
};

// =====================================================
// INITIALIZATION
// =====================================================

void irrigationInit();

// =====================================================
// UPDATE
// =====================================================

void irrigationUpdate(
    const RTCData& rtc,
    const SoilData& soil,
    const RainData& rain,
    const FlowData& flow,
    const WaterLevelData& waterLevel
);

// =====================================================
// RTC CORRECTION SAFETY
// =====================================================

void irrigationNotifyTimeCorrection(
    const RTCData& rtc
);

// =====================================================
// IRRIGATION STATE
// =====================================================

bool irrigationIsActive();

IrrigationState irrigationGetState();

// =====================================================
// SESSION STATISTICS
// =====================================================

// Volume sesi yang sedang berjalan atau sesi terakhir.
float irrigationGetSessionLitres();

// Biaya sesi yang sedang berjalan atau sesi terakhir.
float irrigationGetSessionCost();

// =====================================================
// DAILY STATISTICS
// =====================================================

// Total volume irigasi pada hari berjalan.
float irrigationGetDailyLitres();

// Total biaya irigasi pada hari berjalan.
float irrigationGetDailyCost();

// =====================================================
// MANUAL SAFETY STOP
// =====================================================

void irrigationEmergencyStop();

#endif // LANDIA_IRRIGATION_CONTROLLER_H
