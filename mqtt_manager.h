#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

#include "dht_sensor.h"
#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"
#include "rtc_manager.h"

// MQTT hanya menangani komunikasi. Kontrol irigasi tetap lokal di ESP32.
// Wi-Fi/MQTT/ACK tidak boleh menjadi syarat scheduler atau relay bekerja.

void mqttInit();
void mqttUpdate();

bool mqttIsConnected();
bool wifiIsConnected();

// Menyimpan telemetry valid ke LittleFS; mqttUpdate mengirim antreannya.
void mqttPublishTelemetry(
    const DHTData &dht,
    const SoilData &soil,
    const RainData &rain,
    const FlowData &flow,
    const WaterLevelData &waterLevel,
    const RTCData &rtc
);

// Menyimpan event irigasi ke antrean event LittleFS.
// Pengiriman menunggu backend memiliki consumer dan application ACK event.
void mqttPublishIrrigationEvent(
    const char *event,
    const char *state,
    const RTCData &rtc
);

#endif // MQTT_MANAGER_H
