#ifndef LANDIA_WATER_LEVEL_H
#define LANDIA_WATER_LEVEL_H

#include <Arduino.h>


// =====================================================
// WATER LEVEL DATA
// =====================================================

struct WaterLevelData {

    float distanceCm;

    float waterLevelPercent;

    bool valid;
};


// =====================================================
// FUNCTIONS
// =====================================================

void waterLevelInit();

WaterLevelData waterLevelRead();


// =====================================================
// INTERNAL FUNCTIONS
// =====================================================

float readDistanceCm();

float calculateWaterLevelPercent(
    float distanceCm
);

float readFilteredDistanceCm();

#endif