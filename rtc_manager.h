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
// INITIALIZATION
// =====================================================

// Inisialisasi komunikasi I2C dan RTC DS3231.
// Jika RTC kehilangan daya, sistem menggunakan waktu
// kompilasi firmware sebagai fallback awal.
void rtcInit();

// =====================================================
// NTP TIME SYNCHRONIZATION
// =====================================================

// Memperbarui DS3231 menggunakan waktu NTP ketika
// Wi-Fi tersedia dan sinkronisasi NTP berhasil.
//
// Return:
// true  = RTC benar-benar dikoreksi.
// false = tidak ada koreksi atau sinkronisasi belum siap.
//
// Fungsi ini tidak menggantikan kontrol lokal ketika offline.
bool rtcNtpSyncUpdate(bool wifiConnected);

// =====================================================
// READ RTC
// =====================================================

// Membaca tanggal dan waktu dari DS3231.
// Field valid menunjukkan apakah data RTC layak digunakan.
RTCData rtcRead();

// =====================================================
// FORMAT DATA
// =====================================================

// Format timestamp: YYYY-MM-DD HH:MM:SS
String rtcGetTimestamp();

// Format tanggal: YYYY-MM-DD
String rtcGetDate();

// Format waktu: HH:MM:SS
String rtcGetTime();

// =====================================================
// RTC STATUS
// =====================================================

// Memeriksa apakah DS3231 berhasil diinisialisasi.
bool rtcIsAvailable();

// =====================================================
// MANUAL RTC SET
// =====================================================

// Mengatur waktu DS3231 secara manual untuk keperluan
// maintenance atau kalibrasi.
//
// Parameter waktu harus menggunakan zona waktu yang
// dipakai sistem, yaitu WIB (UTC+7).
//
// Fungsi ini tidak dipanggil otomatis oleh sistem utama.
bool rtcSetDateTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
);

#endif // LANDIA_RTC_MANAGER_H