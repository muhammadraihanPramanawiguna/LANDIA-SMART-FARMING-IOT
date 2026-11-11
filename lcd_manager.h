#ifndef LANDIA_LCD_MANAGER_H
#define LANDIA_LCD_MANAGER_H

#include <Arduino.h>

// =====================================================
// INITIALIZATION
// =====================================================

void lcdInit();

// =====================================================
// LCD UPDATE
// =====================================================

void lcdUpdate(

    float temperature,
    float humidity,

    float soilPercent,
    float rainPercent,

    float flowRate,
    float totalLitres,

    float waterLevelPercent,

    float sessionLitres,
    float sessionCost,

    bool pumpOn,
    bool irrigationActive,

    bool rtcValid,

    int hour,
    int minute,
    int second
);

#endif