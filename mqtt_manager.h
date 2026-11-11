#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

#include "dht_sensor.h"
#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"
#include "rtc_manager.h"

// ============================================================
// MQTT MANAGER
// Communication layer untuk Landia Farm
//
// Prinsip:
// - MQTT tidak mengontrol irrigation controller
// - MQTT tidak boleh memblokir loop utama
// - WiFi/MQTT failure tidak menghentikan local control
// ============================================================

void mqttInit();

void mqttUpdate();

bool mqttIsConnected();
bool wifiIsConnected();

// Publish telemetry seluruh sensor
void mqttPublishTelemetry(
    const DHTData &dht,
    const SoilData &soil,
    const RainData &rain,
    const FlowData &flow,
    const WaterLevelData &waterLevel,
    const RTCData &rtc
);

// Publish event irrigation
void mqttPublishIrrigationEvent(
    const char *event,
    const char *state,
    const RTCData &rtc
);

#endif