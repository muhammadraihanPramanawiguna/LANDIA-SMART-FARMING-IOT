#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

#include "dht_sensor.h"
#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"
#include "rtc_manager.h"

// Komunikasi MQTT ThingsBoard. Kontrol irigasi tetap lokal.
// mqttUpdate() hanya mengelola antrean dan status koneksi;
// tidak menunggu Wi-Fi/broker secara sinkron.

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

// Event irigasi disimpan ke antrean event LittleFS.
// Belum dipublikasikan sampai schema/consumer event dikonfirmasi.
void mqttPublishIrrigationEvent(
    const char *event,
    const char *state,
    const RTCData &rtc
);

#endif // MQTT_MANAGER_H
