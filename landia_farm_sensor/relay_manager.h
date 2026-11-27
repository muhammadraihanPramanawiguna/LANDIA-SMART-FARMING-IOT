#ifndef LANDIA_RELAY_MANAGER_H
#define LANDIA_RELAY_MANAGER_H

#include <Arduino.h>

// =====================================================
// RELAY FUNCTIONS
// =====================================================

void relayInit();

void relayOn();

void relayOff();

bool relayIsOn();

#endif