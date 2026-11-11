#ifndef LANDIA_SOIL_SENSOR_H
#define LANDIA_SOIL_SENSOR_H

#include <Arduino.h>

struct SoilData {
    int rawValue;
    float moisturePercent;
    bool valid;
};

void soilInit();
SoilData soilRead();

#endif