
#include "dht_sensor.h"
#include "config.h"

#include <DHT.h>
#include <math.h>

// =====================================================
// DHT CONFIGURATION
// =====================================================

#define DHT_TYPE DHT11

// Object DHT.
static DHT dht(
    DHT_PIN,
    DHT_TYPE
);


// =====================================================
// SENSOR DATA VALIDATION
// =====================================================

static bool isValidDHTData(
    float temperature,
    float humidity
) {

    return
        isfinite(temperature) &&
        isfinite(humidity) &&

        // Batas kewajaran data suhu.
        temperature >= -40.0f &&
        temperature <= 80.0f &&

        // Kelembapan relatif harus 0-100%.
        humidity >= 0.0f &&
        humidity <= 100.0f;
}


// =====================================================
// INITIALIZATION
// =====================================================

void dhtInit() {

    dht.begin();

    Serial.println(
        "[DHT] DHT11 sensor initialized"
    );

    Serial.print(
        "[DHT] GPIO: "
    );

    Serial.println(DHT_PIN);
}


// =====================================================
// READ SENSOR
// =====================================================

DHTData dhtRead() {

    DHTData data = {};

    // -------------------------------------------------
    // DEFAULT DATA
    // -------------------------------------------------

    data.temperature = NAN;

    data.humidity = NAN;

    data.valid = false;


    // -------------------------------------------------
    // READ HUMIDITY
    // -------------------------------------------------

    const float humidity =
        dht.readHumidity();


    // -------------------------------------------------
    // READ TEMPERATURE
    // -------------------------------------------------

    const float temperature =
        dht.readTemperature();


    // -------------------------------------------------
    // VALIDATE SENSOR DATA
    // -------------------------------------------------

    if (
        !isValidDHTData(
            temperature,
            humidity
        )
    ) {

        Serial.println(
            "[DHT] Invalid sensor reading"
        );

        return data;
    }


    // -------------------------------------------------
    // SAVE VALID DATA
    // -------------------------------------------------

    data.temperature = temperature;

    data.humidity = humidity;

    data.valid = true;


    return data;
}
