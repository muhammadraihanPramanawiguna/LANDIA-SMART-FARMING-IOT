
#ifndef LANDIA_SOIL_SENSOR_H
#define LANDIA_SOIL_SENSOR_H

#include <Arduino.h>

// =====================================================
// SOIL SENSOR DATA
// =====================================================

struct SoilData {
    int rawValue;
    float moisturePercent;
    bool valid;
};

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void soilInit();
SoilData soilRead();

#endif
