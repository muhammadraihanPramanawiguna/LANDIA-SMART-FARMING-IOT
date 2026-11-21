#ifndef LANDIA_WATER_LEVEL_H
#define LANDIA_WATER_LEVEL_H

#include <Arduino.h>

struct WaterLevelData {

    float distanceCm;

    float waterLevelPercent;

    bool valid;
};

void waterLevelInit();

WaterLevelData waterLevelRead();

float readDistanceCm();

float calculateWaterLevelPercent(
    float distanceCm
);

float readFilteredDistanceCm();

#endif