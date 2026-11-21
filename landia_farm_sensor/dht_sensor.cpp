#include "dht_sensor.h"
#include "config.h"

#include <DHT.h>
#include <math.h>

// =====================================================
// DHT CONFIGURATION
// =====================================================

// Sensor yang digunakan pada konfigurasi Landia Farm.
#define DHT_TYPE DHT11

static DHT dht(DHT_PIN, DHT_TYPE);


// =====================================================
// SENSOR DATA VALIDATION
// =====================================================

static bool isValidDHTData(
    float temperature,
    float humidity
) {

    // Batas suhu dibuat mengikuti rentang operasional
    // DHT11, bukan rentang sensor suhu generik.
    return
        isfinite(temperature) &&
        isfinite(humidity) &&
        temperature >= 0.0f &&
        temperature <= 50.0f &&
        humidity >= 0.0f &&
        humidity <= 100.0f;
}


// =====================================================
// INITIALIZATION
// =====================================================

void dhtInit() {

    dht.begin();

    Serial.println("[DHT] DHT11 sensor initialized");
    Serial.print("[DHT] GPIO: ");
    Serial.println(DHT_PIN);
}


// =====================================================
// READ SENSOR
// =====================================================

DHTData dhtRead() {

    DHTData data = {};

    data.temperature = NAN;
    data.humidity = NAN;
    data.valid = false;

    // Adafruit DHT library menerapkan interval baca internal.
    // Pembacaan humidity dan temperature berurutan akan
    // menggunakan cache pembacaan yang sama bila masih valid.
    const float humidity = dht.readHumidity();
    const float temperature = dht.readTemperature();

    if (!isValidDHTData(temperature, humidity)) {

        Serial.println("[DHT] Invalid sensor reading");
        return data;
    }

    data.temperature = temperature;
    data.humidity = humidity;
    data.valid = true;

    return data;
}
