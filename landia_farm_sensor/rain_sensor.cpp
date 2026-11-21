
#include "rain_sensor.h"
#include "config.h"

// =====================================================
// RAIN SENSOR CONFIGURATION
// =====================================================

// ESP32 ADC default range: 0 - 4095.
static constexpr int RAIN_ADC_MAX = 4095;

// Jumlah sampel untuk mengurangi noise pembacaan.
static constexpr int RAIN_SAMPLE_COUNT = 5;

// Threshold sementara.
// Harus divalidasi melalui pengujian sensor fisik.
static constexpr float RAIN_DETECTION_THRESHOLD = 10.0f;


// =====================================================
// READ ADC MEDIAN
// =====================================================

static int readRainMedian() {

    int samples[RAIN_SAMPLE_COUNT];

    // -------------------------------------------------
    // READ SAMPLES
    // -------------------------------------------------

    for (int i = 0; i < RAIN_SAMPLE_COUNT; i++) {

        samples[i] = analogRead(RAIN_PIN);

        delayMicroseconds(2000);
    }

    // -------------------------------------------------
    // SORT SAMPLES
    // -------------------------------------------------

    for (int i = 0; i < RAIN_SAMPLE_COUNT - 1; i++) {

        for (int j = 0; j < RAIN_SAMPLE_COUNT - i - 1; j++) {

            if (samples[j] > samples[j + 1]) {

                int temporary = samples[j];

                samples[j] = samples[j + 1];

                samples[j + 1] = temporary;
            }
        }
    }

    // -------------------------------------------------
    // RETURN MEDIAN
    // -------------------------------------------------

    return samples[RAIN_SAMPLE_COUNT / 2];
}


// =====================================================
// INITIALIZATION
// =====================================================

void rainInit() {

    pinMode(RAIN_PIN, INPUT);

    Serial.println("[RAIN] Sensor initialized");

    Serial.println(
        "[RAIN] ADC range: 0 - 4095"
    );

    Serial.println(
        "[RAIN] Detection threshold: 10% (temporary)"
    );
}


// =====================================================
// READ SENSOR
// =====================================================

RainData rainRead() {

    RainData data = {};

    // -------------------------------------------------
    // READ ANALOG SENSOR
    // -------------------------------------------------

    const int rawValue = readRainMedian();

    data.rawValue = rawValue;

    // -------------------------------------------------
    // VALIDATE ADC VALUE
    // -------------------------------------------------

    if (
        rawValue < 0 ||
        rawValue > RAIN_ADC_MAX
    ) {

        data.valid = false;

        return data;
    }

    // -------------------------------------------------
    // CONVERT ADC TO NORMALIZED PERCENTAGE
    // -------------------------------------------------

    // Mapping sementara:
    // ADC 4095 = 0%
    // ADC 0    = 100%
    //
    // Arah mapping harus divalidasi dengan sensor fisik.

    data.rainPercent =
        (
            static_cast<float>(
                RAIN_ADC_MAX - rawValue
            ) /
            static_cast<float>(RAIN_ADC_MAX)
        ) * 100.0f;

    // -------------------------------------------------
    // LIMIT PERCENTAGE
    // -------------------------------------------------

    if (data.rainPercent < 0.0f) {

        data.rainPercent = 0.0f;
    }

    if (data.rainPercent > 100.0f) {

        data.rainPercent = 100.0f;
    }

    // -------------------------------------------------
    // RAIN DETECTION
    // -------------------------------------------------

    // Status hujan hanya untuk monitoring.
    // Tidak digunakan sebagai penghalang irigasi.

    data.raining =
        data.rainPercent >
        RAIN_DETECTION_THRESHOLD;

    // -------------------------------------------------
    // DATA VALID
    // -------------------------------------------------

    data.valid = true;

    return data;
}
