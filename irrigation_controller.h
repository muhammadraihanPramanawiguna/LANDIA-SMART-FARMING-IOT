
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

// Inisialisasi pengendali irigasi.
// Relay harus berada dalam kondisi OFF saat boot.
void irrigationInit();

// =====================================================
// UPDATE
// =====================================================

// Evaluasi jadwal, sensor, dan kondisi keselamatan irigasi.
// Dipanggil secara berkala dari main.ino.
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

// Dipanggil ketika RTC benar-benar dikoreksi oleh NTP.
// Digunakan untuk mencegah koreksi waktu memicu jadwal
// irigasi secara tidak sengaja.
void irrigationNotifyTimeCorrection(
    const RTCData& rtc
);

// =====================================================
// IRRIGATION STATE
// =====================================================

// Memeriksa apakah irigasi sedang aktif.
bool irrigationIsActive();

// Mengambil status irigasi saat ini.
IrrigationState irrigationGetState();

// =====================================================
// SESSION DATA
// =====================================================

// Mengambil total liter air pada sesi irigasi terakhir.
float irrigationGetSessionLitres();

// Mengambil estimasi biaya sesi irigasi terakhir.
float irrigationGetSessionCost();

// =====================================================
// MANUAL SAFETY STOP
// =====================================================

// Menghentikan irigasi secara paksa.
void irrigationEmergencyStop();

#endif // IRRIGATION_CONTROLLER_H
