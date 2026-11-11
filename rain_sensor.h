#ifndef LANDIA_RAIN_SENSOR_H
#define LANDIA_RAIN_SENSOR_H

#include <Arduino.h>


// =====================================================
// RAIN SENSOR DATA
// =====================================================

struct RainData {

    int rawValue;

    float rainPercent;

    bool raining;

    bool valid;
};


// =====================================================
// FUNCTIONS
// =====================================================

void rainInit();

RainData rainRead();


#endif