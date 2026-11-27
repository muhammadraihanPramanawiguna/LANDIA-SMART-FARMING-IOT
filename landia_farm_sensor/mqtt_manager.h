#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

#include "dht_sensor.h"
#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"
#include "rtc_manager.h"

// MQTT ThingsBoard. Kontrol irigasi tetap lokal.
// mqttUpdate() bersifat non-blocking terhadap proses reconnect.
void mqttInit();
void mqttUpdate();

bool mqttIsConnected();
bool wifiIsConnected();

void mqttPublishTelemetry(
    const DHTData &dht,
    const SoilData &soil,
    const RainData &rain,
    const FlowData &flow,
    const WaterLevelData &waterLevel,
    const RTCData &rtc
);

// Event irigasi masih disimpan lokal sampai schema event server dikonfirmasi.
void mqttPublishIrrigationEvent(
    const char *event,
    const char *state,
    const RTCData &rtc
);

#endif // MQTT_MANAGER_H
