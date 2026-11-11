#ifndef LANDIA_DHT_SENSOR_H
#define LANDIA_DHT_SENSOR_H

#include <Arduino.h>

// Data yang dihasilkan sensor DHT11
struct DHTData {

    float temperature;
    float humidity;

    bool valid;
};


// Inisialisasi DHT11
void dhtInit();


// Membaca DHT11
DHTData dhtRead();

#endif