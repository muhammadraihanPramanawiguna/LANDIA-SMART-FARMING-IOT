#include "rtc_manager.h"

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <esp_sntp.h>

#include "config.h"

// =====================================================
// RTC OBJECT
// =====================================================

static RTC_DS3231 rtc;

// =====================================================
// RTC STATUS
// =====================================================

static bool rtcAvailable = false;

// =====================================================
// NTP STATE
// =====================================================

static bool ntpConfigured = false;
static bool ntpWasConnected = false;

static unsigned long lastNtpAttemptMillis = 0;
static unsigned long lastNtpSuccessMillis = 0;

static const unsigned long NTP_RETRY_INTERVAL_MS =
    5000UL;

static const unsigned long NTP_SYNC_INTERVAL_MS =
    21600000UL; // 6 jam

// =====================================================
// HELPER: GET MONTH NUMBER FROM COMPILE DATE
// =====================================================

static int getCompileMonth(const char* month) {
    const char* months[] = {
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"
    };

    for (int i = 0; i < 12; i++) {
        if (strncmp(month, months[i], 3) == 0) {
            return i + 1;
        }
    }

    return 1;
}

// =====================================================
// SET RTC FROM FIRMWARE COMPILE TIME
// =====================================================

// Fallback ketika RTC kehilangan daya.
// Waktu ini berasal dari komputer saat firmware di-compile.

static bool rtcSetFromCompileTime() {
    char monthText[4] = {0};

    int day = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (
        sscanf(
            __DATE__,
            "%3s %d %d",
            monthText,
            &day,
            &year
        ) != 3
    ) {
        Serial.println(
            "[RTC] ERROR: Cannot parse compile date"
        );
        return false;
    }

    if (
        sscanf(
            __TIME__,
            "%d:%d:%d",
            &hour,
            &minute,
            &second
        ) != 3
    ) {
        Serial.println(
            "[RTC] ERROR: Cannot parse compile time"
        );
        return false;
    }

    const int month = getCompileMonth(monthText);

    if (
        year < 2020 ||
        year > 2099 ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > 31 ||
        hour < 0 ||
        hour > 23 ||
        minute < 0 ||
        minute > 59 ||
        second < 0 ||
        second > 59
    ) {
        Serial.println(
            "[RTC] ERROR: Invalid compile time"
        );
        return false;
    }

    rtc.adjust(
        DateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
    );

    Serial.println(
        "[RTC] RTC initialized from firmware compile time"
    );

    Serial.print("[RTC] Timestamp: ");
    Serial.println(rtcGetTimestamp());

    return true;
}

// =====================================================
// INITIALIZATION
// =====================================================

void rtcInit() {
    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    if (!rtc.begin()) {
        rtcAvailable = false;

        Serial.println(
            "[RTC] ERROR: DS3231 not detected!"
        );

        return;
    }

    rtcAvailable = true;

    Serial.println("[RTC] DS3231 initialized");

    if (rtc.lostPower()) {
        Serial.println(
            "[RTC] WARNING: RTC lost power"
        );

        Serial.println(
            "[RTC] Using firmware compile time as fallback"
        );

        if (rtcSetFromCompileTime()) {
            Serial.println(
                "[RTC] Compile-time fallback successful"
            );
        } else {
            Serial.println(
                "[RTC] ERROR: Compile-time fallback failed"
            );
        }
    } else {
        Serial.println(
            "[RTC] Existing RTC time retained"
        );
    }
}

// =====================================================
// RTC AVAILABILITY
// =====================================================

bool rtcIsAvailable() {
    return rtcAvailable;
}

// =====================================================
// READ RTC
// =====================================================

RTCData rtcRead() {
    RTCData data = {};

    data.year = 0;
    data.month = 0;
    data.day = 0;

    data.hour = 0;
    data.minute = 0;
    data.second = 0;

    data.valid = false;

    if (!rtcAvailable) {
        return data;
    }

    const DateTime now = rtc.now();

    data.year = now.year();
    data.month = now.month();
    data.day = now.day();

    data.hour = now.hour();
    data.minute = now.minute();
    data.second = now.second();

    if (
        data.year < 2020 ||
        data.year > 2099 ||
        data.month < 1 ||
        data.month > 12 ||
        data.day < 1 ||
        data.day > 31 ||
        data.hour < 0 ||
        data.hour > 23 ||
        data.minute < 0 ||
        data.minute > 59 ||
        data.second < 0 ||
        data.second > 59
    ) {
        data.valid = false;
        return data;
    }

    data.valid = true;

    return data;
}

// =====================================================
// GET TIMESTAMP
// =====================================================

String rtcGetTimestamp() {
    if (!rtcAvailable) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    char timestamp[20];

    snprintf(
        timestamp,
        sizeof(timestamp),
        "%04d-%02d-%02d %02d:%02d:%02d",
        now.year(),
        now.month(),
        now.day(),
        now.hour(),
        now.minute(),
        now.second()
    );

    return String(timestamp);
}

// =====================================================
// GET DATE
// =====================================================

String rtcGetDate() {
    if (!rtcAvailable) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    char dateBuffer[11];

    snprintf(
        dateBuffer,
        sizeof(dateBuffer),
        "%04d-%02d-%02d",
        now.year(),
        now.month(),
        now.day()
    );

    return String(dateBuffer);
}

// =====================================================
// GET TIME
// =====================================================

String rtcGetTime() {
    if (!rtcAvailable) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    char timeBuffer[9];

    snprintf(
        timeBuffer,
        sizeof(timeBuffer),
        "%02d:%02d:%02d",
        now.hour(),
        now.minute(),
        now.second()
    );

    return String(timeBuffer);
}

// =====================================================
// MANUAL SET RTC
// =====================================================

// Untuk maintenance atau pengaturan waktu manual.

bool rtcSetDateTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
) {
    if (!rtcAvailable) {
        Serial.println(
            "[RTC] ERROR: RTC unavailable"
        );

        return false;
    }

    if (
        year < 2020 ||
        year > 2099 ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > 31 ||
        hour < 0 ||
        hour > 23 ||
        minute < 0 ||
        minute > 59 ||
        second < 0 ||
        second > 59
    ) {
        Serial.println(
            "[RTC] ERROR: Invalid manual date/time"
        );

        return false;
    }

    rtc.adjust(
        DateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
    );

    Serial.println(
        "[RTC] Time successfully updated manually"
    );

    Serial.print("[RTC] Timestamp: ");
    Serial.println(rtcGetTimestamp());

    return true;
}

// =====================================================
// NTP TIME SYNCHRONIZATION
// =====================================================

// Return true hanya jika RTC benar-benar dikoreksi.
// Fungsi ini tidak menggantikan kontrol lokal ketika offline.

bool rtcNtpSyncUpdate(bool wifiConnected) {
    if (!rtcAvailable) {
        return false;
    }

    // -------------------------------------------------
    // WIFI DISCONNECTED
    // -------------------------------------------------

    if (!wifiConnected) {
        if (ntpWasConnected) {
            ntpConfigured = false;
            lastNtpAttemptMillis = 0;

            Serial.println(
                "[NTP] Wi-Fi disconnected; retaining RTC time"
            );
        }

        ntpWasConnected = false;

        return false;
    }

    // -------------------------------------------------
    // WIFI RECONNECTED
    // -------------------------------------------------

    if (!ntpWasConnected) {
        ntpWasConnected = true;
        ntpConfigured = false;
        lastNtpAttemptMillis = 0;

        Serial.println(
            "[NTP] Wi-Fi connected; preparing synchronization"
        );
    }

    // -------------------------------------------------
    // CONFIGURE NTP
    // -------------------------------------------------

    if (!ntpConfigured) {
        configTime(
            7 * 3600,
            0,
            "pool.ntp.org",
            "time.google.com",
            "time.cloudflare.com"
        );

        ntpConfigured = true;

        Serial.println(
            "[NTP] Configured for WIB (UTC+7)"
        );
    }

    const unsigned long nowMillis = millis();

    // -------------------------------------------------
    // PERIODIC SYNCHRONIZATION
    // -------------------------------------------------

    if (
        lastNtpSuccessMillis != 0 &&
        nowMillis - lastNtpSuccessMillis <
            NTP_SYNC_INTERVAL_MS
    ) {
        return false;
    }

    // -------------------------------------------------
    // RETRY LIMIT
    // -------------------------------------------------

    if (
        lastNtpAttemptMillis != 0 &&
        nowMillis - lastNtpAttemptMillis <
            NTP_RETRY_INTERVAL_MS
    ) {
        return false;
    }

    lastNtpAttemptMillis = nowMillis;

    // -------------------------------------------------
    // REQUIRE ACTUAL SNTP SYNCHRONIZATION
    // -------------------------------------------------

    // getLocalTime() saja tidak membuktikan bahwa waktu
    // baru diterima dari server NTP. Waktu sistem bisa
    // saja masih berasal dari nilai sebelumnya.

    if (
        sntp_get_sync_status() !=
        SNTP_SYNC_STATUS_COMPLETED
    ) {
        Serial.println(
            "[NTP] Waiting for successful SNTP synchronization"
        );

        return false;
    }

    // -------------------------------------------------
    // READ SYSTEM TIME
    // -------------------------------------------------

    struct tm ntpTime = {};

    if (!getLocalTime(&ntpTime, 10)) {
        Serial.println(
            "[NTP] System time unavailable; retaining RTC"
        );

        return false;
    }

    const int year = ntpTime.tm_year + 1900;
    const int month = ntpTime.tm_mon + 1;
    const int day = ntpTime.tm_mday;

    const int hour = ntpTime.tm_hour;
    const int minute = ntpTime.tm_min;
    const int second = ntpTime.tm_sec;

    // -------------------------------------------------
    // VALIDATE NTP TIME
    // -------------------------------------------------

    if (
        year < 2024 ||
        year > 2099 ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > 31 ||
        hour < 0 ||
        hour > 23 ||
        minute < 0 ||
        minute > 59 ||
        second < 0 ||
        second > 59
    ) {
        Serial.println(
            "[NTP] Invalid time rejected"
        );

        return false;
    }

    const DateTime ntpDateTime(
        year,
        month,
        day,
        hour,
        minute,
        second
    );

    const DateTime rtcDateTime = rtc.now();

    // Kedua DateTime dibandingkan dalam representasi
    // tanggal/jam lokal yang sama (WIB).
    long differenceSeconds =
        (long)ntpDateTime.unixtime() -
        (long)rtcDateTime.unixtime();

    if (differenceSeconds < 0) {
        differenceSeconds = -differenceSeconds;
    }

    Serial.print("[NTP] RTC difference: ");
    Serial.print(differenceSeconds);
    Serial.println(" seconds");

    // -------------------------------------------------
    // TIME ALREADY ACCURATE
    // -------------------------------------------------

    if (differenceSeconds <= 5) {
        lastNtpSuccessMillis = nowMillis;

        Serial.println(
            "[NTP] RTC already accurate; no correction needed"
        );

        return false;
    }

    // -------------------------------------------------
    // CORRECT RTC
    // -------------------------------------------------

    rtc.adjust(ntpDateTime);

    lastNtpSuccessMillis = millis();

    Serial.print("[NTP] RTC corrected to WIB: ");
    Serial.println(rtcGetTimestamp());

    return true;
}
