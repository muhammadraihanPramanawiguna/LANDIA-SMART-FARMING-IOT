
#include <Arduino.h>
#include <math.h>

#include "water_level.h"
#include "config.h"

// =====================================================
// SENSOR CONFIGURATION
// =====================================================

// Jumlah sampel pembacaan.
static constexpr int JSN_FILTER_SAMPLES = 5;

// Minimum jumlah sampel valid yang dibutuhkan.
static constexpr int JSN_MIN_VALID_SAMPLES = 3;

// Jarak minimum pembacaan JSN-SR04M.
// Perlu disesuaikan dengan varian sensor yang digunakan.
static constexpr float JSN_MIN_VALID_DISTANCE_CM = 20.0f;

// Timeout pembacaan ECHO.
static constexpr unsigned long JSN_TIMEOUT_US = 30000UL;

// Jeda antarpengukuran untuk mengurangi pantulan
// dari pengukuran sebelumnya.
static constexpr unsigned long JSN_SAMPLE_INTERVAL_MS = 60UL;


// =====================================================
// CONFIGURATION VALIDATION
// =====================================================

static bool isValidWaterLevelConfig() {

    return
        isfinite(WATER_LEVEL_FULL_DISTANCE_CM) &&
        isfinite(WATER_LEVEL_EMPTY_DISTANCE_CM) &&

        WATER_LEVEL_FULL_DISTANCE_CM >=
            JSN_MIN_VALID_DISTANCE_CM &&

        WATER_LEVEL_EMPTY_DISTANCE_CM >
            WATER_LEVEL_FULL_DISTANCE_CM;
}


// =====================================================
// INITIALIZATION
// =====================================================

void waterLevelInit() {

    pinMode(JSN_TRIG_PIN, OUTPUT);

    pinMode(JSN_ECHO_PIN, INPUT);

    // Pastikan TRIG LOW saat inisialisasi.
    digitalWrite(JSN_TRIG_PIN, LOW);

    Serial.println(
        "[JSN] Water level sensor initialized"
    );

    Serial.print("[JSN] Full distance: ");

    Serial.print(
        WATER_LEVEL_FULL_DISTANCE_CM,
        1
    );

    Serial.println(" cm");

    Serial.print("[JSN] Empty distance: ");

    Serial.print(
        WATER_LEVEL_EMPTY_DISTANCE_CM,
        1
    );

    Serial.println(" cm");

    Serial.print("[JSN] Minimum valid distance: ");

    Serial.print(
        JSN_MIN_VALID_DISTANCE_CM,
        1
    );

    Serial.println(" cm");

    if (!isValidWaterLevelConfig()) {

        Serial.println(
            "[JSN] ERROR: Invalid water level configuration"
        );
    }
}


// =====================================================
// READ SINGLE DISTANCE
// =====================================================

float readDistanceCm() {

    // -------------------------------------------------
    // TRIGGER LOW
    // -------------------------------------------------

    digitalWrite(JSN_TRIG_PIN, LOW);

    delayMicroseconds(2);

    // -------------------------------------------------
    // TRIGGER HIGH
    // -------------------------------------------------

    digitalWrite(JSN_TRIG_PIN, HIGH);

    delayMicroseconds(10);

    // -------------------------------------------------
    // TRIGGER LOW
    // -------------------------------------------------

    digitalWrite(JSN_TRIG_PIN, LOW);

    // -------------------------------------------------
    // READ ECHO
    // -------------------------------------------------

    const unsigned long duration = pulseIn(
        JSN_ECHO_PIN,
        HIGH,
        JSN_TIMEOUT_US
    );

    // -------------------------------------------------
    // TIMEOUT VALIDATION
    // -------------------------------------------------

    if (duration == 0) {

        return -1.0f;
    }

    // -------------------------------------------------
    // CONVERT DURATION TO DISTANCE
    // -------------------------------------------------

    const float distance =
        static_cast<float>(duration) *
        0.0343f /
        2.0f;

    // -------------------------------------------------
    // DISTANCE VALIDATION
    // -------------------------------------------------

    if (
        !isfinite(distance) ||
        distance < JSN_MIN_VALID_DISTANCE_CM
    ) {

        return -1.0f;
    }

    return distance;
}


// =====================================================
// SORT DISTANCES
// =====================================================

static void sortDistances(
    float values[],
    int count
) {

    for (int i = 0; i < count - 1; i++) {

        for (int j = i + 1; j < count; j++) {

            if (values[j] < values[i]) {

                const float temporary = values[i];

                values[i] = values[j];

                values[j] = temporary;
            }
        }
    }
}


// =====================================================
// READ FILTERED DISTANCE
// =====================================================

float readFilteredDistanceCm() {

    float samples[JSN_FILTER_SAMPLES];

    int validSamples = 0;

    // -------------------------------------------------
    // TAKE MULTIPLE READINGS
    // -------------------------------------------------

    for (
        int i = 0;
        i < JSN_FILTER_SAMPLES;
        i++
    ) {

        const float distance = readDistanceCm();

        if (
            isfinite(distance) &&
            distance >= JSN_MIN_VALID_DISTANCE_CM
        ) {

            samples[validSamples] = distance;

            validSamples++;
        }

        // Jeda antar pembacaan.
        delay(JSN_SAMPLE_INTERVAL_MS);
    }

    // -------------------------------------------------
    // VALIDATE SAMPLE COUNT
    // -------------------------------------------------

    if (
        validSamples < JSN_MIN_VALID_SAMPLES
    ) {

        Serial.print("[JSN] Insufficient valid samples: ");

        Serial.println(validSamples);

        return -1.0f;
    }

    // -------------------------------------------------
    // SORT VALID SAMPLES
    // -------------------------------------------------

    sortDistances(
        samples,
        validSamples
    );

    // -------------------------------------------------
    // CALCULATE MEDIAN
    // -------------------------------------------------

    const int middle = validSamples / 2;

    if (validSamples % 2 == 1) {

        return samples[middle];
    }

    return (
        samples[middle - 1] +
        samples[middle]
    ) / 2.0f;
}


// =====================================================
// CALCULATE WATER LEVEL PERCENTAGE
// =====================================================

float calculateWaterLevelPercent(
    float distanceCm
) {

    // -------------------------------------------------
    // VALIDATE INPUT
    // -------------------------------------------------

    if (
        !isfinite(distanceCm) ||
        distanceCm < 0.0f ||
        !isValidWaterLevelConfig()
    ) {

        return -1.0f;
    }

    // -------------------------------------------------
    // FULL CONDITION
    // -------------------------------------------------

    if (
        distanceCm <=
        WATER_LEVEL_FULL_DISTANCE_CM
    ) {

        return 100.0f;
    }

    // -------------------------------------------------
    // EMPTY CONDITION
    // -------------------------------------------------

    if (
        distanceCm >=
        WATER_LEVEL_EMPTY_DISTANCE_CM
    ) {

        return 0.0f;
    }

    // -------------------------------------------------
    // LINEAR INTERPOLATION
    // -------------------------------------------------

    const float waterLevelPercent =
        (
            (
                WATER_LEVEL_EMPTY_DISTANCE_CM -
                distanceCm
            ) /
            (
                WATER_LEVEL_EMPTY_DISTANCE_CM -
                WATER_LEVEL_FULL_DISTANCE_CM
            )
        ) * 100.0f;

    if (!isfinite(waterLevelPercent)) {

        return -1.0f;
    }

    // -------------------------------------------------
    // LIMIT PERCENTAGE
    // -------------------------------------------------

    return constrain(
        waterLevelPercent,
        0.0f,
        100.0f
    );
}


// =====================================================
// READ WATER LEVEL
// =====================================================

WaterLevelData waterLevelRead() {

    WaterLevelData data = {};

    data.distanceCm = -1.0f;

    data.waterLevelPercent = 0.0f;

    data.valid = false;

    // -------------------------------------------------
    // CONFIGURATION VALIDATION
    // -------------------------------------------------

    if (!isValidWaterLevelConfig()) {

        Serial.println(
            "[JSN] Invalid sensor configuration"
        );

        return data;
    }

    // -------------------------------------------------
    // READ FILTERED DISTANCE
    // -------------------------------------------------

    const float distance =
        readFilteredDistanceCm();

    // -------------------------------------------------
    // INVALID DISTANCE
    // -------------------------------------------------

    if (
        !isfinite(distance) ||
        distance < 0.0f
    ) {

        Serial.println(
            "[JSN] Invalid distance reading"
        );

        return data;
    }

    // -------------------------------------------------
    // CALCULATE WATER LEVEL
    // -------------------------------------------------

    const float percentage =
        calculateWaterLevelPercent(distance);

    if (
        !isfinite(percentage) ||
        percentage < 0.0f ||
        percentage > 100.0f
    ) {

        Serial.println(
            "[JSN] Invalid water level percentage"
        );

        return data;
    }

    // -------------------------------------------------
    // SAVE VALID DATA
    // -------------------------------------------------

    data.distanceCm = distance;

    data.waterLevelPercent = percentage;

    data.valid = true;

    return data;
}
