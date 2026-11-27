#ifndef LANDIA_DHT_SENSOR_H
#define LANDIA_DHT_SENSOR_H

#include <Arduino.h>

// =====================================================
// DHT SENSOR DATA
// =====================================================

struct DHTData {

    float temperature;
    float humidity;

    bool valid;
};


// =====================================================
// DHT DIAGNOSTICS
// =====================================================

enum DHTErrorType {

    DHT_ERROR_NONE,
    DHT_ERROR_NAN,
    DHT_ERROR_TEMPERATURE_RANGE,
    DHT_ERROR_HUMIDITY_RANGE,
    DHT_ERROR_UNKNOWN
};


struct DHTDiagnostics {

    // Status pembacaan terakhir
    bool lastReadValid;

    // Jumlah kegagalan berturut-turut
    uint32_t consecutiveFailures;

    // Total pembacaan gagal sejak perangkat menyala
    uint32_t totalFailures;

    // Total pembacaan berhasil sejak perangkat menyala
    uint32_t totalSuccesses;

    // Data terakhir yang berhasil dibaca
    float lastValidTemperature;
    float lastValidHumidity;

    // Waktu sukses terakhir dalam millis()
    // 0 berarti belum pernah berhasil membaca
    uint32_t lastSuccessfulReadMillis;

    // Jenis error terakhir
    DHTErrorType lastError;
};


// =====================================================
// PUBLIC FUNCTIONS
// =====================================================

// Inisialisasi sensor
void dhtInit();

// Membaca sensor DHT11
DHTData dhtRead();

// Mengambil informasi diagnostik sensor
DHTDiagnostics dhtGetDiagnostics();

// Mengubah error menjadi teks
const char* dhtGetErrorMessage(DHTErrorType error);


#endif