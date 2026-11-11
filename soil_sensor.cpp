#include "soil_sensor.h"
#include "config.h"


// =====================================================
// INITIALIZATION
// =====================================================

void soilInit() {

    pinMode(SOIL_PIN, INPUT);

    Serial.println(
        "[SOIL] Sensor initialized"
    );
}


// =====================================================
// READ SENSOR
// =====================================================

SoilData soilRead() {

    SoilData data;

    data.rawValue = 0;
    data.moisturePercent = 0.0;
    data.valid = false;


    // Baca ADC ESP32
    int rawValue = analogRead(SOIL_PIN);


    // Simpan nilai ADC
    data.rawValue = rawValue;


    // -------------------------------------------------
    // Sementara kita gunakan mapping dasar.
    //
    // INI BELUM NILAI KALIBRASI FINAL LANDIA FARM.
    // Nanti kita kalibrasi menggunakan tanah/media
    // tanam sebenarnya.
    // -------------------------------------------------

    data.moisturePercent =
        map(
            rawValue,
            4095,
            0,
            0,
            100
        );


    // Batasi 0 - 100%
    if (data.moisturePercent < 0) {
        data.moisturePercent = 0;
    }

    if (data.moisturePercent > 100) {
        data.moisturePercent = 100;
    }


    data.valid = true;


    return data;
}