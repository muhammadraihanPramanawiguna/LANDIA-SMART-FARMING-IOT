#include "soil_sensor.h"
#include "config.h"

#include <math.h>

// =====================================================
// SOIL SENSOR CONFIGURATION
// =====================================================

// Jumlah sampel untuk Median Filter.
static constexpr uint8_t SOIL_SAMPLE_COUNT = 5;

// Jeda antar pembacaan ADC dalam mikrodetik.
static constexpr unsigned long SOIL_SAMPLE_DELAY_US = 2000UL;

// =====================================================
// ADC CALIBRATION
// =====================================================

// Mapping sementara:
// 4095 = kondisi kering
// 0    = kondisi basah
//
// Nilai ini belum merupakan kalibrasi final
// untuk media tanam Landia Farm.
static constexpr int SOIL_ADC_DRY_RAW = 4095;
static constexpr int SOIL_ADC_WET_RAW = 0;

// =====================================================
// CALIBRATION VALIDATION
// =====================================================

static bool isValidSoilCalibration() {
    return
        SOIL_ADC_DRY_RAW >= 0 &&
        SOIL_ADC_DRY_RAW <= 4095 &&
        SOIL_ADC_WET_RAW >= 0 &&
        SOIL_ADC_WET_RAW <= 4095 &&
        SOIL_ADC_DRY_RAW != SOIL_ADC_WET_RAW;
}

// =====================================================
// MEDIAN FILTER
// =====================================================

static int getMedianValue(int values[], uint8_t count) {
    if (count == 0 || count > SOIL_SAMPLE_COUNT) {
        return -1;
    }

    int sortedValues[SOIL_SAMPLE_COUNT];

    for (uint8_t i = 0; i < count; i++) {
        sortedValues[i] = values[i];
    }

    // Bubble Sort sederhana; jumlah sampel kecil (5).
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (sortedValues[j] > sortedValues[j + 1]) {
                const int temporary = sortedValues[j];
                sortedValues[j] = sortedValues[j + 1];
                sortedValues[j + 1] = temporary;
            }
        }
    }

    return sortedValues[count / 2];
}

// =====================================================
// CONVERT ADC TO MOISTURE PERCENTAGE
// =====================================================

static float convertToMoisturePercent(int rawValue) {
    if (!isValidSoilCalibration() ||
        rawValue < 0 ||
        rawValue > 4095) {
        return -1.0f;
    }

    const float denominator =
        static_cast<float>(SOIL_ADC_DRY_RAW - SOIL_ADC_WET_RAW);

    if (denominator == 0.0f) {
        return -1.0f;
    }

    float moisturePercent =
        (static_cast<float>(SOIL_ADC_DRY_RAW - rawValue) /
         denominator) * 100.0f;

    if (!isfinite(moisturePercent)) {
        return -1.0f;
    }

    // Batasi hasil ke rentang 0–100%.
    if (moisturePercent < 0.0f) {
        moisturePercent = 0.0f;
    } else if (moisturePercent > 100.0f) {
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

    if (!isValidSoilCalibration()) {
        Serial.println("[SOIL] ERROR: Invalid ADC calibration");
    } else {
        Serial.printf(
            "[SOIL] Calibration: dry=%d, wet=%d\\n",
            SOIL_ADC_DRY_RAW,
            SOIL_ADC_WET_RAW
        );
    }
}

// =====================================================
// READ SENSOR
// =====================================================

SoilData soilRead() {
    SoilData data = {};

    data.rawValue = 0;
    data.moisturePercent = 0.0f;
    data.valid = false;

    // Pastikan konfigurasi kalibrasi valid.
    if (!isValidSoilCalibration()) {
        Serial.println("[SOIL] ERROR: Invalid calibration configuration");
        return data;
    }

    // -------------------------------------------------
    // 1. READ MULTIPLE ADC SAMPLES
    // -------------------------------------------------

    int samples[SOIL_SAMPLE_COUNT];

    for (uint8_t i = 0; i < SOIL_SAMPLE_COUNT; i++) {
        samples[i] = analogRead(SOIL_PIN);

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

    const int filteredRawValue =
        getMedianValue(samples, SOIL_SAMPLE_COUNT);

    if (filteredRawValue < 0) {
        Serial.println("[SOIL] ERROR: Median filter failed");
        return data;
    }

    data.rawValue = filteredRawValue;

    // -------------------------------------------------
    // 4. CONVERT TO MOISTURE PERCENTAGE
    // -------------------------------------------------

    const float moisturePercent =
        convertToMoisturePercent(filteredRawValue);

    // -------------------------------------------------
    // 5. VALIDATE RESULT
    // -------------------------------------------------

    if (!isfinite(moisturePercent) ||
        moisturePercent < 0.0f ||
        moisturePercent > 100.0f) {
        Serial.println("[SOIL] ERROR: Invalid moisture percentage");
        return data;
    }

    data.moisturePercent = moisturePercent;
    data.valid = true;

    return data;
}
