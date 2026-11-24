
#ifndef LANDIA_FLOW_SENSOR_H
#define LANDIA_FLOW_SENSOR_H

#include <Arduino.h>
#include <stdint.h>

// =====================================================
// FLOW SENSOR DATA
// =====================================================

struct FlowData {

    // Pulsa yang terdeteksi pada interval pengukuran terakhir.
    unsigned long pulseCount;

    // Akumulasi seluruh pulsa sejak total di-reset.
    uint64_t totalPulseCount;

    // Kecepatan aliran air (L/min).
    float flowRateLMin;

    // Volume kumulatif (liter).
    float totalLitres;

    // Volume kumulatif (m3).
    float totalCubicMeter;

    // Biaya kumulatif (Rupiah).
    float totalCost;

    // Validitas hasil pengukuran.
    bool valid;
};

// =====================================================
// FUNCTIONS
// =====================================================

void flowInit();

void flowUpdate();

FlowData flowRead();

// =====================================================
// TOTAL RESET
// =====================================================

// Mengatur ulang volume kumulatif dan menyimpan
// nilai nol ke NVS.
void flowResetTotal();

// =====================================================
// INTERRUPT
// =====================================================

void IRAM_ATTR flowPulseCounter();

#endif
