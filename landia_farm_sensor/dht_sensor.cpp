#include "dht_sensor.h"
#include "config.h"

#include <DHT.h>
#include <math.h>


// =====================================================
// DHT CONFIGURATION
// =====================================================

#define DHT_TYPE DHT11

static DHT dht(DHT_PIN, DHT_TYPE);


// =====================================================
// DIAGNOSTIC STATE
// =====================================================

static DHTDiagnostics diagnostics = {

    false,      // lastReadValid

    0,          // consecutiveFailures
    0,          // totalFailures
    0,          // totalSuccesses

    NAN,        // lastValidTemperature
    NAN,        // lastValidHumidity

    0,          // lastSuccessfulReadMillis

    DHT_ERROR_NONE
};


// =====================================================
// SENSOR VALIDATION
// =====================================================

// Validasi nilai suhu DHT11
static bool isValidTemperature(float temperature) {

    return
        isfinite(temperature) &&
        temperature >= 0.0f &&
        temperature <= 50.0f;
}


// Validasi nilai kelembapan DHT11
static bool isValidHumidity(float humidity) {

    return
        isfinite(humidity) &&
        humidity >= 0.0f &&
        humidity <= 100.0f;
}


// =====================================================
// ERROR CLASSIFICATION
// =====================================================

static DHTErrorType classifyDHTError(
    float temperature,
    float humidity
) {

    // Sensor tidak berhasil menghasilkan nilai valid
    if (
        !isfinite(temperature) ||
        !isfinite(humidity)
    ) {

        return DHT_ERROR_NAN;
    }


    // Suhu berada di luar rentang valid
    if (!isValidTemperature(temperature)) {

        return DHT_ERROR_TEMPERATURE_RANGE;
    }


    // Kelembapan berada di luar rentang valid
    if (!isValidHumidity(humidity)) {

        return DHT_ERROR_HUMIDITY_RANGE;
    }


    return DHT_ERROR_NONE;
}


// =====================================================
// ERROR MESSAGE
// =====================================================

const char* dhtGetErrorMessage(DHTErrorType error) {

    switch (error) {

        case DHT_ERROR_NONE:

            return "No error";


        case DHT_ERROR_NAN:

            return "NaN / communication failure";


        case DHT_ERROR_TEMPERATURE_RANGE:

            return "Temperature out of range";


        case DHT_ERROR_HUMIDITY_RANGE:

            return "Humidity out of range";


        case DHT_ERROR_UNKNOWN:

            return "Unknown error";


        default:

            return "Unknown error";
    }
}


// =====================================================
// INITIALIZATION
// =====================================================

void dhtInit() {

    dht.begin();

    // Reset diagnostic state saat perangkat mulai
    diagnostics.lastReadValid = false;

    diagnostics.consecutiveFailures = 0;
    diagnostics.totalFailures = 0;
    diagnostics.totalSuccesses = 0;

    diagnostics.lastValidTemperature = NAN;
    diagnostics.lastValidHumidity = NAN;

    diagnostics.lastSuccessfulReadMillis = 0;

    diagnostics.lastError = DHT_ERROR_NONE;


    Serial.println();
    Serial.println("================================");
    Serial.println("[DHT] SENSOR INITIALIZATION");
    Serial.println("================================");

    Serial.println("[DHT] Sensor       : DHT11");

    Serial.print("[DHT] GPIO         : ");
    Serial.println(DHT_PIN);

    Serial.println("[DHT] Initialization completed");
    Serial.println("================================");
}


// =====================================================
// READ SENSOR
// =====================================================

DHTData dhtRead() {

    DHTData data = {};

    data.temperature = NAN;
    data.humidity = NAN;
    data.valid = false;


    // =================================================
    // READ RAW SENSOR DATA
    // =================================================

    const float humidity = dht.readHumidity();

    const float temperature = dht.readTemperature();


    // =================================================
    // VALIDATION
    // =================================================

    const DHTErrorType error = classifyDHTError(
        temperature,
        humidity
    );


    // =================================================
    // FAILED READING
    // =================================================

    if (error != DHT_ERROR_NONE) {

        diagnostics.lastReadValid = false;

        diagnostics.consecutiveFailures++;

        diagnostics.totalFailures++;

        diagnostics.lastError = error;


        Serial.println();
        Serial.println("[DHT] READ FAILED");

        Serial.print("[DHT] Error         : ");
        Serial.println(dhtGetErrorMessage(error));


        Serial.print("[DHT] Temperature   : ");

        if (isfinite(temperature)) {

            Serial.print(temperature);
            Serial.println(" C");
        }

        else {

            Serial.println("NaN");
        }


        Serial.print("[DHT] Humidity      : ");

        if (isfinite(humidity)) {

            Serial.print(humidity);
            Serial.println(" %");
        }

        else {

            Serial.println("NaN");
        }


        Serial.print("[DHT] Consecutive failures : ");
        Serial.println(diagnostics.consecutiveFailures);


        Serial.print("[DHT] Total failures       : ");
        Serial.println(diagnostics.totalFailures);


        // Data tetap invalid
        // Jangan menggunakan data terakhir sebagai
        // pengganti data pembacaan saat ini.

        return data;
    }


    // =================================================
    // SUCCESSFUL READING
    // =================================================

    data.temperature = temperature;
    data.humidity = humidity;
    data.valid = true;


    // =================================================
    // UPDATE DIAGNOSTICS
    // =================================================

    diagnostics.lastReadValid = true;

    diagnostics.consecutiveFailures = 0;

    diagnostics.totalSuccesses++;

    diagnostics.lastValidTemperature = temperature;

    diagnostics.lastValidHumidity = humidity;

    diagnostics.lastSuccessfulReadMillis = millis();

    diagnostics.lastError = DHT_ERROR_NONE;


    // =================================================
    // SERIAL OUTPUT
    // =================================================

    Serial.println();
    Serial.println("[DHT] READ SUCCESS");

    Serial.print("[DHT] Temperature : ");
    Serial.print(temperature);
    Serial.println(" C");


    Serial.print("[DHT] Humidity    : ");
    Serial.print(humidity);
    Serial.println(" %");


    Serial.print("[DHT] Success count : ");
    Serial.println(diagnostics.totalSuccesses);


    Serial.println("[DHT] Consecutive failures reset");


    return data;
}


// =====================================================
// GET DIAGNOSTICS
// =====================================================

DHTDiagnostics dhtGetDiagnostics() {

    return diagnostics;
}