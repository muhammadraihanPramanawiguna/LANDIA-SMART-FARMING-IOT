#include <Arduino.h>
#include "config.h"
#include "relay_manager.h"

// =====================================================
// RELAY STATE
// =====================================================

static bool relayState = false;


// =====================================================
// RELAY INITIALIZATION
// =====================================================

void relayInit() {

    pinMode(RELAY_PIN, OUTPUT);

    // Relay OFF saat ESP32 pertama kali menyala
    relayOff();
}


// =====================================================
// RELAY ON
// =====================================================

void relayOn() {

    digitalWrite(RELAY_PIN, LOW);

    relayState = true;
}


// =====================================================
// RELAY OFF
// =====================================================

void relayOff() {

    digitalWrite(RELAY_PIN, HIGH);

    relayState = false;
}


// =====================================================
// GET RELAY STATE
// =====================================================

bool relayIsOn() {

    return relayState;
}