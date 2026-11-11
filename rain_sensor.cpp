#include "rain_sensor.h"
#include "config.h"


// =====================================================
// INITIALIZATION
// =====================================================

void rainInit() {

    pinMode(
        RAIN_PIN,
        INPUT
    );

    Serial.println(
        "[RAIN] Sensor initialized"
    );
}


// =====================================================
// READ SENSOR
// =====================================================

RainData rainRead() {

    RainData data;

    data.rawValue = 0;
    data.rainPercent = 0.0;
    data.raining = false;
    data.valid = false;


    // -------------------------------------------------
    // Read analog value
    // -------------------------------------------------

    int rawValue =
        analogRead(RAIN_PIN);


    data.rawValue =
        rawValue;


    // -------------------------------------------------
    // Convert ADC to percentage
    //
    // TEMPORARY MAPPING
    // -------------------------------------------------

    data.rainPercent =
        map(
            rawValue,
            4095,
            0,
            0,
            100
        );


    // -------------------------------------------------
    // Limit percentage
    // -------------------------------------------------

    if (data.rainPercent < 0) {

        data.rainPercent = 0;
    }


    if (data.rainPercent > 100) {

        data.rainPercent = 100;
    }


    // -------------------------------------------------
    // Temporary rain detection
    //
    // Nanti threshold kita tentukan setelah
    // hardware benar-benar dites.
    // -------------------------------------------------

    if (data.rainPercent > 10) {

        data.raining = true;

    } else {

        data.raining = false;
    }


    data.valid = true;


    return data;
}