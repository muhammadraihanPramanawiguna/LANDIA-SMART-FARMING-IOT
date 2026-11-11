#include "dht_sensor.h"
#include "config.h"

#include <DHT.h>


// =====================================================
// DHT CONFIGURATION
// =====================================================

#define DHT_TYPE DHT11


// Object DHT
DHT dht(
    DHT_PIN,
    DHT_TYPE
);


// =====================================================
// INITIALIZATION
// =====================================================

void dhtInit() {

    dht.begin();

    Serial.println(
        "[DHT] Sensor initialized"
    );
}


// =====================================================
// READ SENSOR
// =====================================================

DHTData dhtRead() {

    DHTData data;

    // Default state
    data.temperature = NAN;
    data.humidity = NAN;
    data.valid = false;


    // Read humidity
    float humidity =
        dht.readHumidity();


    // Read temperature
    float temperature =
        dht.readTemperature();


    // Check invalid reading
    if (
        isnan(humidity) ||
        isnan(temperature)
    ) {

        Serial.println(
            "[DHT] Failed to read sensor"
        );

        return data;
    }


    // Store valid data
    data.temperature =
        temperature;

    data.humidity =
        humidity;

    data.valid =
        true;


    return data;
}