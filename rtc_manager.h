#ifndef LANDIA_RTC_MANAGER_H
#define LANDIA_RTC_MANAGER_H

#include <Arduino.h>

// =====================================================
// RTC DATA
// =====================================================

struct RTCData {
    int year;
    int month;
    int day;

    int hour;
    int minute;
    int second;

    bool valid;
};

// =====================================================
// RTC INITIALIZATION
// =====================================================

void rtcInit();

// =====================================================
// RTC STATUS
// =====================================================

bool rtcIsAvailable();

// =====================================================
// RTC READ
// =====================================================

RTCData rtcRead();

// =====================================================
// RTC DATE AND TIME STRING
// =====================================================

String rtcGetTimestamp();

String rtcGetDate();

String rtcGetTime();

// =====================================================
// MANUAL RTC CONFIGURATION
// =====================================================

bool rtcSetDateTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
);

// =====================================================
// NTP SYNCHRONIZATION
// =====================================================

// Return true hanya jika RTC benar-benar dikoreksi.
// Parameter false digunakan ketika Wi-Fi terputus.

bool rtcNtpSyncUpdate(bool wifiConnected);

#endif