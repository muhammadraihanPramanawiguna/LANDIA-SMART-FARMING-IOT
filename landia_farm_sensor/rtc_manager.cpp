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

// Menunjukkan apakah perangkat DS3231 berhasil dideteksi.
static bool rtcAvailable = false;

// Menunjukkan apakah waktu di dalam DS3231 sudah valid.
static bool rtcTimeValid = false;

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

static const long NTP_TIMEZONE_OFFSET_SECONDS =
    7L * 3600L; // WIB UTC+7

static const int RTC_MIN_YEAR = 2020;
static const int RTC_MAX_YEAR = 2099;

// =====================================================
// HELPER: LEAP YEAR
// =====================================================

static bool isLeapYear(int year) {

    return (
        (year % 400 == 0) ||
        (
            year % 4 == 0 &&
            year % 100 != 0
        )
    );
}

// =====================================================
// HELPER: DAYS IN MONTH
// =====================================================

static int getDaysInMonth(
    int year,
    int month
) {

    static const int daysPerMonth[] = {
        31, 28, 31, 30,
        31, 30, 31, 31,
        30, 31, 30, 31
    };

    if (month < 1 || month > 12) {
        return 0;
    }

    if (
        month == 2 &&
        isLeapYear(year)
    ) {
        return 29;
    }

    return daysPerMonth[month - 1];
}

// =====================================================
// HELPER: VALIDATE DATE AND TIME
// =====================================================

static bool isValidDateTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
) {

    if (
        year < RTC_MIN_YEAR ||
        year > RTC_MAX_YEAR
    ) {
        return false;
    }

    if (
        month < 1 ||
        month > 12
    ) {
        return false;
    }

    const int maxDays = getDaysInMonth(
        year,
        month
    );

    if (
        day < 1 ||
        day > maxDays
    ) {
        return false;
    }

    if (
        hour < 0 ||
        hour > 23
    ) {
        return false;
    }

    if (
        minute < 0 ||
        minute > 59
    ) {
        return false;
    }

    if (
        second < 0 ||
        second > 59
    ) {
        return false;
    }

    return true;
}

// =====================================================
// HELPER: VALIDATE RTC DATE OBJECT
// =====================================================

static bool isValidRTCDateTime(
    const DateTime& dateTime
) {

    return isValidDateTime(
        dateTime.year(),
        dateTime.month(),
        dateTime.day(),
        dateTime.hour(),
        dateTime.minute(),
        dateTime.second()
    );
}

// =====================================================
// HELPER: GET MONTH NUMBER FROM COMPILE DATE
// =====================================================

static int getCompileMonth(
    const char* month
) {

    const char* months[] = {
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"
    };

    for (int i = 0; i < 12; i++) {

        if (
            strncmp(
                month,
                months[i],
                3
            ) == 0
        ) {
            return i + 1;
        }
    }

    return 0;
}

// =====================================================
// SET RTC FROM FIRMWARE COMPILE TIME
// =====================================================

// Fallback ketika RTC kehilangan daya.
//
// Catatan:
// Waktu ini berasal dari komputer saat firmware
// di-compile. Pastikan timezone komputer WIB.

static bool rtcSetFromCompileTime() {

    char monthText[4] = {0};

    int day = 0;
    int year = 0;

    int hour = 0;
    int minute = 0;
    int second = 0;

    // -------------------------------------------------
    // PARSE COMPILE DATE
    // -------------------------------------------------

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

    // -------------------------------------------------
    // PARSE COMPILE TIME
    // -------------------------------------------------

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

    const int month = getCompileMonth(
        monthText
    );

    // -------------------------------------------------
    // VALIDATE COMPILE TIME
    // -------------------------------------------------

    if (
        !isValidDateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
    ) {

        Serial.println(
            "[RTC] ERROR: Invalid compile date/time"
        );

        return false;
    }

    // -------------------------------------------------
    // UPDATE RTC
    // -------------------------------------------------

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

    rtcTimeValid = true;

    Serial.println(
        "[RTC] RTC initialized from firmware compile time"
    );

    Serial.print(
        "[RTC] Timestamp: "
    );

    Serial.println(
        rtcGetTimestamp()
    );

    return true;
}

// =====================================================
// INITIALIZATION
// =====================================================

void rtcInit() {

    rtcAvailable = false;
    rtcTimeValid = false;

    ntpConfigured = false;
    ntpWasConnected = false;

    lastNtpAttemptMillis = 0;
    lastNtpSuccessMillis = 0;

    // -------------------------------------------------
    // INITIALIZE I2C
    // -------------------------------------------------

    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    // -------------------------------------------------
    // INITIALIZE DS3231
    // -------------------------------------------------

    if (!rtc.begin()) {

        rtcAvailable = false;
        rtcTimeValid = false;

        Serial.println(
            "[RTC] ERROR: DS3231 not detected!"
        );

        return;
    }

    rtcAvailable = true;

    Serial.println(
        "[RTC] DS3231 initialized"
    );

    // -------------------------------------------------
    // CHECK POWER LOSS
    // -------------------------------------------------

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

            rtcTimeValid = false;

            Serial.println(
                "[RTC] ERROR: Compile-time fallback failed"
            );

            Serial.println(
                "[RTC] Waiting for valid NTP or manual time"
            );
        }

    } else {

        // -------------------------------------------------
        // CHECK EXISTING RTC TIME
        // -------------------------------------------------

        const DateTime now = rtc.now();

        if (isValidRTCDateTime(now)) {

            rtcTimeValid = true;

            Serial.println(
                "[RTC] Existing RTC time retained"
            );

        } else {

            rtcTimeValid = false;

            Serial.println(
                "[RTC] WARNING: Existing RTC time is invalid"
            );
        }
    }

    // -------------------------------------------------
    // DISPLAY RTC STATUS
    // -------------------------------------------------

    if (rtcTimeValid) {

        Serial.print(
            "[RTC] Current time: "
        );

        Serial.println(
            rtcGetTimestamp()
        );

    } else {

        Serial.println(
            "[RTC] Time is not valid yet"
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

    if (
        !rtcAvailable ||
        !rtcTimeValid
    ) {
        return data;
    }

    const DateTime now = rtc.now();

    // -------------------------------------------------
    // VALIDATE RTC TIME
    // -------------------------------------------------

    if (!isValidRTCDateTime(now)) {

        rtcTimeValid = false;

        Serial.println(
            "[RTC] ERROR: Invalid RTC date/time detected"
        );

        return data;
    }

    // -------------------------------------------------
    // COPY RTC DATA
    // -------------------------------------------------

    data.year = now.year();
    data.month = now.month();
    data.day = now.day();

    data.hour = now.hour();
    data.minute = now.minute();
    data.second = now.second();

    data.valid = true;

    return data;
}

// =====================================================
// GET TIMESTAMP
// =====================================================

String rtcGetTimestamp() {

    if (
        !rtcAvailable ||
        !rtcTimeValid
    ) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    if (!isValidRTCDateTime(now)) {

        rtcTimeValid = false;

        return "INVALID";
    }

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

    if (
        !rtcAvailable ||
        !rtcTimeValid
    ) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    if (!isValidRTCDateTime(now)) {

        rtcTimeValid = false;

        return "INVALID";
    }

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

    if (
        !rtcAvailable ||
        !rtcTimeValid
    ) {
        return "INVALID";
    }

    const DateTime now = rtc.now();

    if (!isValidRTCDateTime(now)) {

        rtcTimeValid = false;

        return "INVALID";
    }

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

// Digunakan untuk maintenance atau pengaturan waktu manual.

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

    // -------------------------------------------------
    // VALIDATE INPUT
    // -------------------------------------------------

    if (
        !isValidDateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
    ) {

        Serial.println(
            "[RTC] ERROR: Invalid manual date/time"
        );

        return false;
    }

    // -------------------------------------------------
    // UPDATE RTC
    // -------------------------------------------------

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

    rtcTimeValid = true;

    // -------------------------------------------------
    // RESET NTP SYNC TIMER
    // -------------------------------------------------

    lastNtpSuccessMillis = 0;

    Serial.println(
        "[RTC] Time successfully updated manually"
    );

    Serial.print(
        "[RTC] Timestamp: "
    );

    Serial.println(
        rtcGetTimestamp()
    );

    return true;
}

// =====================================================
// NTP TIME SYNCHRONIZATION
// =====================================================

// Return true hanya jika RTC benar-benar dikoreksi.
//
// Jika Wi-Fi terputus:
// - RTC tetap digunakan.
// - Tidak ada perubahan waktu RTC.
//
// Jika Wi-Fi tersedia:
// - ESP32 mencoba sinkronisasi NTP.
// - RTC dikoreksi jika selisih waktu lebih dari 5 detik.

bool rtcNtpSyncUpdate(
    bool wifiConnected
) {

    if (!rtcAvailable) {
        return false;
    }

    // =================================================
    // WIFI DISCONNECTED
    // =================================================

    if (!wifiConnected) {

        if (ntpWasConnected) {

            ntpConfigured = false;

            lastNtpAttemptMillis = 0;

            // Memastikan sinkronisasi dapat dicoba
            // kembali setelah Wi-Fi tersambung.

            lastNtpSuccessMillis = 0;

            Serial.println(
                "[NTP] Wi-Fi disconnected; retaining RTC time"
            );
        }

        ntpWasConnected = false;

        return false;
    }

    // =================================================
    // WIFI RECONNECTED
    // =================================================

    if (!ntpWasConnected) {

        ntpWasConnected = true;

        ntpConfigured = false;

        lastNtpAttemptMillis = 0;

        lastNtpSuccessMillis = 0;

        Serial.println(
            "[NTP] Wi-Fi connected; preparing synchronization"
        );
    }

    // =================================================
    // CONFIGURE NTP
    // =================================================

    if (!ntpConfigured) {

        configTime(
            NTP_TIMEZONE_OFFSET_SECONDS,
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

    // =================================================
    // PERIODIC SYNCHRONIZATION
    // =================================================

    if (
        lastNtpSuccessMillis != 0 &&
        nowMillis - lastNtpSuccessMillis <
            NTP_SYNC_INTERVAL_MS
    ) {
        return false;
    }

    // =================================================
    // RETRY LIMIT
    // =================================================

    if (
        lastNtpAttemptMillis != 0 &&
        nowMillis - lastNtpAttemptMillis <
            NTP_RETRY_INTERVAL_MS
    ) {
        return false;
    }

    lastNtpAttemptMillis = nowMillis;

    // =================================================
    // REQUIRE SUCCESSFUL SNTP SYNCHRONIZATION
    // =================================================

    // getLocalTime() saja tidak membuktikan bahwa waktu
    // baru diterima dari server NTP.
    //
    // Karena itu status SNTP juga harus diperiksa.

    if (
        sntp_get_sync_status() !=
        SNTP_SYNC_STATUS_COMPLETED
    ) {

        Serial.println(
            "[NTP] Waiting for successful SNTP synchronization"
        );

        return false;
    }

    // =================================================
    // READ SYSTEM TIME
    // =================================================

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

    // =================================================
    // VALIDATE NTP TIME
    // =================================================

    if (
        !isValidDateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
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

    // =================================================
    // RTC TIME NOT VALID
    // =================================================

    // Jika DS3231 terdeteksi tetapi waktunya invalid,
    // waktu NTP yang valid dapat langsung digunakan.

    if (!rtcTimeValid) {

        rtc.adjust(ntpDateTime);

        rtcTimeValid = true;

        lastNtpSuccessMillis = millis();

        Serial.println(
            "[NTP] RTC initialized using NTP time"
        );

        Serial.print(
            "[NTP] RTC time: "
        );

        Serial.println(
            rtcGetTimestamp()
        );

        return true;
    }

    // =================================================
    // READ CURRENT RTC TIME
    // =================================================

    const DateTime rtcDateTime = rtc.now();

    if (!isValidRTCDateTime(rtcDateTime)) {

        rtcTimeValid = false;

        Serial.println(
            "[NTP] RTC time invalid; waiting for next synchronization"
        );

        return false;
    }

    // =================================================
    // CALCULATE TIME DIFFERENCE
    // =================================================

    // Kedua DateTime menggunakan representasi waktu lokal
    // yang sama, yaitu WIB.

    long differenceSeconds =
        (long)ntpDateTime.unixtime() -
        (long)rtcDateTime.unixtime();

    if (differenceSeconds < 0) {
        differenceSeconds = -differenceSeconds;
    }

    Serial.print(
        "[NTP] RTC difference: "
    );

    Serial.print(
        differenceSeconds
    );

    Serial.println(
        " seconds"
    );

    // =================================================
    // TIME ALREADY ACCURATE
    // =================================================

    if (differenceSeconds <= 5) {

        lastNtpSuccessMillis = millis();

        Serial.println(
            "[NTP] RTC already accurate; no correction needed"
        );

        return false;
    }

    // =================================================
    // CORRECT RTC
    // =================================================

    rtc.adjust(ntpDateTime);

    rtcTimeValid = true;

    lastNtpSuccessMillis = millis();

    Serial.print(
        "[NTP] RTC corrected to WIB: "
    );

    Serial.println(
        rtcGetTimestamp()
    );

    return true;
}