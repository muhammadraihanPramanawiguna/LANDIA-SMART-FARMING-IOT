
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

// =====================================================
// SENSOR INTERVAL
// =====================================================

// DHT11: temperatur dan kelembapan udara.
constexpr unsigned long DHT_INTERVAL_MS = 5000UL;

// Soil Moisture: pembacaan kelembapan tanah.
constexpr unsigned long SOIL_INTERVAL_MS = 10000UL;

// Rain Sensor: monitoring kondisi hujan.
constexpr unsigned long RAIN_INTERVAL_MS = 10000UL;

// Flow Sensor: monitoring aliran dan volume air.
constexpr unsigned long FLOW_INTERVAL_MS = 1000UL;

// Water Level: monitoring ketinggian air tangki.
constexpr unsigned long WATER_LEVEL_INTERVAL_MS = 10000UL;

// =====================================================
// SYSTEM INTERVAL
// =====================================================

// RTC: update waktu untuk scheduler dan controller.
constexpr unsigned long RTC_INTERVAL_MS = 1000UL;

// LCD: update tampilan.
constexpr unsigned long LCD_INTERVAL_MS = 2000UL;

// Serial: update log monitoring.
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
