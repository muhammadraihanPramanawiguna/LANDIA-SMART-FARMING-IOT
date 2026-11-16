
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

// =====================================================
// SENSOR INTERVAL
// =====================================================

constexpr unsigned long DHT_INTERVAL_MS = 5000UL;
constexpr unsigned long SOIL_INTERVAL_MS = 10000UL;
constexpr unsigned long RAIN_INTERVAL_MS = 10000UL;
constexpr unsigned long FLOW_INTERVAL_MS = 1000UL;
constexpr unsigned long WATER_LEVEL_INTERVAL_MS = 10000UL;

// =====================================================
// SYSTEM INTERVAL
// =====================================================

constexpr unsigned long RTC_INTERVAL_MS = 1000UL;
constexpr unsigned long LCD_INTERVAL_MS = 2000UL;
constexpr unsigned long SERIAL_INTERVAL_MS = 5000UL;

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
