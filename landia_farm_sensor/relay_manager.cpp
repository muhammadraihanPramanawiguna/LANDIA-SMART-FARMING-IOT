
#include <Arduino.h>

#include "config.h"
#include "relay_manager.h"

// =====================================================
// RELAY STATE
// =====================================================

static bool relayState = false;


// =====================================================
// RELAY OUTPUT CONFIGURATION
// =====================================================

// Relay active-low:
// LOW  = ON
// HIGH = OFF

static constexpr uint8_t RELAY_ON_LEVEL =
    RELAY_ACTIVE_LOW ? LOW : HIGH;

static constexpr uint8_t RELAY_OFF_LEVEL =
    RELAY_ACTIVE_LOW ? HIGH : LOW;


// =====================================================
// RELAY INITIALIZATION
// =====================================================

void relayInit() {

    // Atur level OFF sebelum pin menjadi OUTPUT
    // untuk mengurangi risiko pulsa ON saat inisialisasi.

    digitalWrite(
        RELAY_PIN,
        RELAY_OFF_LEVEL
    );

    pinMode(
        RELAY_PIN,
        OUTPUT
    );

    relayState = false;

    Serial.println(
        "[RELAY] Relay initialized"
    );

    Serial.println(
        "[RELAY] Initial state: OFF"
    );
}


// =====================================================
// RELAY ON
// =====================================================

void relayOn() {

    digitalWrite(
        RELAY_PIN,
        RELAY_ON_LEVEL
    );

    relayState = true;

    Serial.println(
        "[RELAY] Pump command: ON"
    );
}


// =====================================================
// RELAY OFF
// =====================================================

void relayOff() {

    digitalWrite(
        RELAY_PIN,
        RELAY_OFF_LEVEL
    );

    relayState = false;

    Serial.println(
        "[RELAY] Pump command: OFF"
    );
}


// =====================================================
// GET RELAY STATE
// =====================================================

bool relayIsOn() {

    return relayState;
}
