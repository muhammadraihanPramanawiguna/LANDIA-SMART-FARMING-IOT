
#include "soil_sensor.h"
#include "config.h"

// =====================================================
// SOIL SENSOR CONFIGURATION
// =====================================================

// Jumlah sampel untuk Median Filter.
// Gunakan angka ganjil agar diperoleh satu nilai tengah.
static constexpr uint8_t SOIL_SAMPLE_COUNT = 5;

// Jeda antar pembacaan ADC dalam mikrodetik.
static constexpr unsigned long SOIL_SAMPLE_DELAY_US = 2000UL;

// =====================================================
// ADC CALIBRATION
// =====================================================

// Mapping sementara dari kode firmware sebelumnya.
//
// CATATAN:
// 4095 = kondisi kering
// 0    = kondisi basah
//
// Nilai ini BELUM merupakan kalibrasi final
// untuk media tanam Landia Farm.

static constexpr int SOIL_ADC_DRY_RAW = 4095;
static constexpr int SOIL_ADC_WET_RAW = 0;

// =====================================================
// MEDIAN FILTER
// =====================================================

static int getMedianValue(int values[], uint8_t count) {

    // Salin data agar array sampel asli tidak berubah.
    int sortedValues[SOIL_SAMPLE_COUNT];

    for (uint8_t i = 0; i < count; i++) {
        sortedValues[i] = values[i];
    }

    // Sorting sederhana menggunakan Bubble Sort.
    for (uint8_t i = 0; i < count - 1; i++) {

        for (uint8_t j = 0; j < count - i - 1; j++) {

            if (sortedValues[j] > sortedValues[j + 1]) {

                int temporary = sortedValues[j];

                sortedValues[j] = sortedValues[j + 1];

                sortedValues[j + 1] = temporary;
            }
        }
    }

    // Mengambil nilai tengah.
    return sortedValues[count / 2];
}

// =====================================================
// CONVERT ADC TO MOISTURE PERCENTAGE
// =====================================================

static float convertToMoisturePercent(int rawValue) {

    float moisturePercent =
        (
            (float)(SOIL_ADC_DRY_RAW - rawValue) /
            (float)(SOIL_ADC_DRY_RAW - SOIL_ADC_WET_RAW)
        ) * 100.0f;

    // Batasi nilai 0 sampai 100 persen.
    if (moisturePercent < 0.0f) {
        moisturePercent = 0.0f;
    }

    if (moisturePercent > 100.0f) {
        moisturePercent = 100.0f;
    }

    return moisturePercent;
}

// =====================================================
// INITIALIZATION
// =====================================================

void soilInit() {

    pinMode(SOIL_PIN, INPUT);

    Serial.println("[SOIL] Sensor initialized");
    Serial.println("[SOIL] Median filter enabled");
}

// =====================================================
// READ SENSOR
// =====================================================

SoilData soilRead() {

    SoilData data;

    data.rawValue = 0;
    data.moisturePercent = 0.0f;
    data.valid = false;

    // -------------------------------------------------
    // 1. READ MULTIPLE ADC SAMPLES
    // -------------------------------------------------

    int samples[SOIL_SAMPLE_COUNT];

    for (uint8_t i = 0; i < SOIL_SAMPLE_COUNT; i++) {

        samples[i] = analogRead(SOIL_PIN);

        // Jeda singkat antar pembacaan.
        if (i < SOIL_SAMPLE_COUNT - 1) {
            delayMicroseconds(SOIL_SAMPLE_DELAY_US);
        }
    }

    // -------------------------------------------------
    // 2. VALIDATE ADC SAMPLES
    // -------------------------------------------------

    for (uint8_t i = 0; i < SOIL_SAMPLE_COUNT; i++) {

        if (samples[i] < 0 || samples[i] > 4095) {

            Serial.println("[SOIL] ERROR: Invalid ADC reading");

            return data;
        }
    }

    // -------------------------------------------------
    // 3. APPLY MEDIAN FILTER
    // -------------------------------------------------

    int filteredRawValue =
        getMedianValue(samples, SOIL_SAMPLE_COUNT);

    data.rawValue = filteredRawValue;

    // -------------------------------------------------
    // 4. CONVERT TO MOISTURE PERCENTAGE
    // -------------------------------------------------

    data.moisturePercent =
        convertToMoisturePercent(filteredRawValue);

    // -------------------------------------------------
    // 5. VALIDATE RESULT
    // -------------------------------------------------

    if (
        data.moisturePercent < 0.0f ||
        data.moisturePercent > 100.0f
    ) {

        Serial.println("[SOIL] ERROR: Invalid moisture percentage");

        return data;
    }

    data.valid = true;

    return data;
}
