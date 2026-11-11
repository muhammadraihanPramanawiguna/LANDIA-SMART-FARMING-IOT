#include <Arduino.h>

#include "water_level.h"
#include "config.h"


// =====================================================
// INTERNAL CONFIGURATION
// =====================================================

// Jumlah pembacaan untuk filtering
#define JSN_FILTER_SAMPLES 5

// JSN-SR04M umumnya kurang reliable pada jarak sangat dekat.
// Nilai ini jangan dibuat lebih kecil dari kemampuan sensor.
#define JSN_MIN_VALID_DISTANCE_CM 20.0f

// Timeout pulse ECHO
#define JSN_TIMEOUT_US 30000UL


// =====================================================
// INITIALIZATION
// =====================================================

void waterLevelInit() {

    pinMode(
        JSN_TRIG_PIN,
        OUTPUT
    );

    pinMode(
        JSN_ECHO_PIN,
        INPUT
    );


    // Pastikan TRIG LOW
    digitalWrite(
        JSN_TRIG_PIN,
        LOW
    );


    Serial.println(
        "[JSN] Water level sensor initialized"
    );


    Serial.print(
        "[JSN] Full distance  : "
    );

    Serial.print(
        WATER_LEVEL_FULL_DISTANCE_CM,
        1
    );

    Serial.println(
        " cm"
    );


    Serial.print(
        "[JSN] Empty distance : "
    );

    Serial.print(
        WATER_LEVEL_EMPTY_DISTANCE_CM,
        1
    );

    Serial.println(
        " cm"
    );


    Serial.print(
        "[JSN] Min valid dist : "
    );

    Serial.print(
        JSN_MIN_VALID_DISTANCE_CM,
        1
    );

    Serial.println(
        " cm"
    );
}


// =====================================================
// READ SINGLE DISTANCE
// =====================================================

float readDistanceCm() {

    // -------------------------------------------------
    // Trigger LOW
    // -------------------------------------------------

    digitalWrite(
        JSN_TRIG_PIN,
        LOW
    );

    delayMicroseconds(2);


    // -------------------------------------------------
    // Trigger HIGH
    // -------------------------------------------------

    digitalWrite(
        JSN_TRIG_PIN,
        HIGH
    );

    delayMicroseconds(10);


    // -------------------------------------------------
    // Trigger LOW
    // -------------------------------------------------

    digitalWrite(
        JSN_TRIG_PIN,
        LOW
    );


    // -------------------------------------------------
    // Read ECHO
    // -------------------------------------------------

    unsigned long duration =
        pulseIn(
            JSN_ECHO_PIN,
            HIGH,
            JSN_TIMEOUT_US
        );


    // -------------------------------------------------
    // Timeout
    // -------------------------------------------------

    if (duration == 0) {

        return -1.0f;
    }


    // -------------------------------------------------
    // Convert duration to centimeter
    // -------------------------------------------------

    float distance =
        duration * 0.0343f / 2.0f;


    // -------------------------------------------------
    // Validate minimum distance
    // -------------------------------------------------

    if (
        distance < JSN_MIN_VALID_DISTANCE_CM
    ) {

        return -1.0f;
    }


    return distance;
}


// =====================================================
// SORT ARRAY
// =====================================================

void sortDistances(
    float values[],
    int count
) {

    for (
        int i = 0;
        i < count - 1;
        i++
    ) {

        for (
            int j = i + 1;
            j < count;
            j++
        ) {

            if (
                values[j] < values[i]
            ) {

                float temp = values[i];

                values[i] = values[j];

                values[j] = temp;
            }
        }
    }
}


// =====================================================
// READ FILTERED DISTANCE
// =====================================================

float readFilteredDistanceCm() {

    float samples[
        JSN_FILTER_SAMPLES
    ];

    int validSamples = 0;


    // -------------------------------------------------
    // Take multiple readings
    // -------------------------------------------------

    for (
        int i = 0;
        i < JSN_FILTER_SAMPLES;
        i++
    ) {

        float distance =
            readDistanceCm();


        if (distance >= 0.0f) {

            samples[
                validSamples
            ] = distance;

            validSamples++;
        }


        // Beri waktu antar pembacaan
        delay(60);
    }


    // -------------------------------------------------
    // Not enough valid samples
    // -------------------------------------------------

    if (
        validSamples == 0
    ) {

        return -1.0f;
    }


    // -------------------------------------------------
    // Sort valid samples
    // -------------------------------------------------

    sortDistances(
        samples,
        validSamples
    );


    // -------------------------------------------------
    // Median
    // -------------------------------------------------

    if (
        validSamples % 2 == 1
    ) {

        // Jumlah ganjil
        return samples[
            validSamples / 2
        ];

    } else {

        // Jumlah genap
        int middle =
            validSamples / 2;

        return (
            samples[middle - 1]
            +
            samples[middle]
        ) / 2.0f;
    }
}


// =====================================================
// CALCULATE WATER LEVEL PERCENTAGE
// =====================================================

float calculateWaterLevelPercent(
    float distanceCm
) {

    // -------------------------------------------------
    // FULL
    // -------------------------------------------------
    //
    // Jika permukaan air lebih dekat atau sama
    // dengan batas FULL:
    //
    // 100%
    //

    if (
        distanceCm <=
        WATER_LEVEL_FULL_DISTANCE_CM
    ) {

        return 100.0f;
    }


    // -------------------------------------------------
    // EMPTY
    // -------------------------------------------------
    //
    // Jika permukaan air lebih jauh atau sama
    // dengan batas EMPTY:
    //
    // 0%
    //

    if (
        distanceCm >=
        WATER_LEVEL_EMPTY_DISTANCE_CM
    ) {

        return 0.0f;
    }


    // -------------------------------------------------
    // LINEAR CALCULATION
    // -------------------------------------------------

    float waterLevelPercent =
        (
            (
                WATER_LEVEL_EMPTY_DISTANCE_CM
                -
                distanceCm
            )
            /
            (
                WATER_LEVEL_EMPTY_DISTANCE_CM
                -
                WATER_LEVEL_FULL_DISTANCE_CM
            )
        )
        * 100.0f;


    // -------------------------------------------------
    // SAFETY LIMIT
    // -------------------------------------------------

    waterLevelPercent =
        constrain(
            waterLevelPercent,
            0.0f,
            100.0f
        );


    return waterLevelPercent;
}


// =====================================================
// READ WATER LEVEL
// =====================================================

WaterLevelData waterLevelRead() {

    WaterLevelData data;


    // -------------------------------------------------
    // DEFAULT DATA
    // -------------------------------------------------

    data.distanceCm =
        -1.0f;

    data.waterLevelPercent =
        0.0f;

    data.valid =
        false;


    // -------------------------------------------------
    // READ FILTERED DISTANCE
    // -------------------------------------------------

    float distance =
        readFilteredDistanceCm();


    // -------------------------------------------------
    // INVALID READING
    // -------------------------------------------------

    if (
        distance < 0.0f
    ) {

        Serial.println(
            "[JSN] Invalid distance reading"
        );

        return data;
    }


    // -------------------------------------------------
    // SAVE DISTANCE
    // -------------------------------------------------

    data.distanceCm =
        distance;


    // -------------------------------------------------
    // CALCULATE WATER LEVEL
    // -------------------------------------------------

    data.waterLevelPercent =
        calculateWaterLevelPercent(
            distance
        );


    // -------------------------------------------------
    // VALID DATA
    // -------------------------------------------------

    data.valid =
        true;


    return data;
}