#include <Arduino.h>

#include "config.h"
#include "relay_manager.h"

// =====================================================
// RELAY STATE
// =====================================================

// Ini adalah state perintah dari ESP32, bukan feedback
// fisik dari kontak relay.
static bool relayState = false;


// =====================================================
// RELAY OUTPUT CONFIGURATION
// =====================================================

static constexpr uint8_t RELAY_ON_LEVEL =
    RELAY_ACTIVE_LOW ? LOW : HIGH;

static constexpr uint8_t RELAY_OFF_LEVEL =
    RELAY_ACTIVE_LOW ? HIGH : LOW;


// =====================================================
// RELAY INITIALIZATION
// =====================================================

void relayInit() {

    // Set output latch ke OFF sebelum pin menjadi OUTPUT.
    // Untuk kondisi sebelum setup(), tetap gunakan pull-up
    // hardware pada input relay active-low.
    digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);

    relayState = false;

    Serial.println("[RELAY] Relay initialized");
    Serial.println("[RELAY] Initial state: OFF");
}


// =====================================================
// RELAY ON
// =====================================================

void relayOn() {

    if (relayState) {
        return;
    }

    digitalWrite(RELAY_PIN, RELAY_ON_LEVEL);
    relayState = true;

    Serial.println("[RELAY] Pump command: ON");
}


// =====================================================
// RELAY OFF
// =====================================================

void relayOff() {

    // Tetap tulis OFF sebagai tindakan fail-safe,
    // meskipun state software sudah OFF.
    digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);

    if (!relayState) {
        return;
    }

    relayState = false;

    Serial.println("[RELAY] Pump command: OFF");
}


// =====================================================
// GET RELAY STATE
// =====================================================

bool relayIsOn() {

    return relayState;
}
