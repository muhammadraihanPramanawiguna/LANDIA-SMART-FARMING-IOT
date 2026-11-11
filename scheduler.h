#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>


// =====================================================
// SENSOR INTERVAL
// =====================================================

#define DHT_INTERVAL_MS            5000UL
#define SOIL_INTERVAL_MS           10000UL
#define RAIN_INTERVAL_MS           10000UL
#define FLOW_INTERVAL_MS           1000UL
#define WATER_LEVEL_INTERVAL_MS    10000UL
#define RTC_INTERVAL_MS            1000UL
#define LCD_INTERVAL_MS            2000UL
#define SERIAL_INTERVAL_MS         5000UL


// =====================================================
// SCHEDULER FUNCTIONS
// =====================================================

void schedulerInit();

bool schedulerDHT();

bool schedulerSoil();

bool schedulerRain();

bool schedulerFlow();

bool schedulerWaterLevel();

bool schedulerRTC();

bool schedulerLCD();

bool schedulerSerial();


#endif