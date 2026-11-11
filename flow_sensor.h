#ifndef LANDIA_FLOW_SENSOR_H
#define LANDIA_FLOW_SENSOR_H

#include <Arduino.h>

// =====================================================
// FLOW SENSOR DATA
// =====================================================

struct FlowData {

    unsigned long pulseCount;

    float flowRateLMin;

    float totalLitres;

    float totalCubicMeter;

    float totalCost;

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

void flowResetTotal();


// =====================================================
// INTERRUPT
// =====================================================

void IRAM_ATTR flowPulseCounter();


#endif