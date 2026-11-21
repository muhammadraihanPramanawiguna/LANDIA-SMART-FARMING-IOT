#include <Arduino.h>
#include <WiFi.h>

// =====================================================
// CONFIGURATION
// =====================================================

#include "config.h"


// =====================================================
// SENSOR MODULES
// =====================================================

#include "dht_sensor.h"
#include "soil_sensor.h"
#include "rain_sensor.h"
#include "flow_sensor.h"
#include "water_level.h"


// =====================================================
// SYSTEM MODULES
// =====================================================

#include "rtc_manager.h"
#include "lcd_manager.h"
#include "relay_manager.h"
#include "scheduler.h"
#include "irrigation_controller.h"


// =====================================================
// COMMUNICATION
// =====================================================

#include "mqtt_manager.h"


// =====================================================
// GLOBAL SENSOR DATA
// =====================================================

DHTData dhtData;

SoilData soilData;

RainData rainData;

FlowData flowData;

WaterLevelData waterLevelData;

RTCData rtcData;


// =====================================================
// TELEMETRY
// =====================================================
//
// MQTT hanya komunikasi.
// Tidak berhubungan dengan keputusan irrigation.
//
// Local control tetap berjalan ketika MQTT/WiFi mati.
// =====================================================

const unsigned long TELEMETRY_INTERVAL = 30000UL;

unsigned long lastTelemetryMillis = 0;


// =====================================================
// SETUP
// =====================================================

void setup()
{
    // =================================================
    // SERIAL
    // =================================================

    Serial.begin(SERIAL_BAUDRATE);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("       LANDIA SMART FARMING");
    Serial.println("       ESP32 IoT CONTROLLER");
    Serial.println("========================================");
    Serial.println();


    // =================================================
    // SENSOR INITIALIZATION
    // =================================================

    Serial.println("[SYSTEM] Initializing sensors...");

    dhtInit();

    soilInit();

    rainInit();

    flowInit();

    waterLevelInit();


    // =================================================
    // RTC
    // =================================================

    Serial.println("[SYSTEM] Initializing RTC...");

    rtcInit();


    // =================================================
    // LCD
    // =================================================

    Serial.println("[SYSTEM] Initializing LCD...");

    lcdInit();


    // =================================================
    // RELAY
    // =================================================

    Serial.println("[SYSTEM] Initializing relay...");

    relayInit();


    // =================================================
    // IRRIGATION
    // =================================================

    Serial.println(
        "[SYSTEM] Initializing irrigation controller..."
    );

    irrigationInit();


    // =================================================
    // SCHEDULER
    // =================================================

    Serial.println("[SYSTEM] Initializing scheduler...");

    schedulerInit();


    // =================================================
    // MQTT
    // =================================================
    //
    // PENTING:
    //
    // mqttInit() TIDAK BOLEH menunggu WiFi.
    //
    // ESP32 langsung selesai boot.
    //
    // Kalau hotspot tersedia:
    //     WiFi -> MQTT -> telemetry
    //
    // Kalau hotspot tidak tersedia:
    //     local control tetap berjalan.
    //
    // =================================================

    Serial.println("[SYSTEM] Initializing MQTT manager...");

    mqttInit();


    // =================================================
    // READY
    // =================================================

    Serial.println();

    Serial.println("----------------------------------------");

    Serial.println(
        "[SYSTEM] Initialization complete."
    );

    Serial.println(
        "[SYSTEM] Sensor monitoring started."
    );

    Serial.println(
        "[SYSTEM] Relay initialized: OFF"
    );

    Serial.println(
        "[SYSTEM] Irrigation controller initialized."
    );

    Serial.println(
        "[SYSTEM] MQTT communication running in background."
    );

    Serial.println(
        "[SYSTEM] Local control does NOT depend on MQTT."
    );

    Serial.println("----------------------------------------");

    Serial.println();
}



void loop() {
    // =================================================
    // 1. DHT11
    // =================================================

    if (schedulerDHT()) {
        dhtData = dhtRead();
    }

    // =================================================
    // 2. SOIL MOISTURE
    // =================================================

    if (schedulerSoil()) {
        soilData = soilRead();
    }

    // =================================================
    // 3. RAIN
    // =================================================

    if (schedulerRain()) {
        rainData = rainRead();
    }

    // =================================================
    // 4. FLOW
    // =================================================

    if (schedulerFlow()) {
        flowUpdate();
        flowData = flowRead();
    }

    // =================================================
    // 5. WATER LEVEL
    // =================================================

    if (schedulerWaterLevel()) {
        waterLevelData = waterLevelRead();
    }

    // =================================================
    // 6. RTC
    // =================================================

    if (schedulerRTC()) {
        rtcData = rtcRead();
    }

    // =================================================
    // 7. MQTT BACKGROUND
    // =================================================
    //
    // Mengelola koneksi Wi-Fi/MQTT.
    // Fungsi harus non-blocking.
    // Kontrol lokal tidak bergantung pada MQTT.
    // =================================================

    mqttUpdate();

    // =================================================
    // 8. NTP BACKGROUND SYNCHRONIZATION
    // =================================================
    //
    // Koreksi RTC sebelum evaluasi jadwal irigasi.
    // Jika offline, RTC tetap digunakan untuk kontrol lokal.
    // =================================================

    if (wifiIsConnected()) {
        if (rtcNtpSyncUpdate(true)) {
            rtcData = rtcRead();

            irrigationNotifyTimeCorrection(rtcData);
        }
    } else {
        // Memberi tahu modul NTP bahwa Wi-Fi terputus.
        // Tidak mengubah waktu DS3231.
        rtcNtpSyncUpdate(false);
    }

    // =================================================
    // 9. LOCAL IRRIGATION CONTROL
    // =================================================
    //
    // Tetap dijalankan tanpa Wi-Fi atau MQTT.
    // irrigationUpdate() memiliki pengamanan untuk RTC
    // dan validitas sensor.
    // =================================================

    irrigationUpdate(
        rtcData,
        soilData,
        rainData,
        flowData,
        waterLevelData
    );

    // =================================================
    // 10. MQTT TELEMETRY
    // =================================================

    if (
        millis() - lastTelemetryMillis >=
        TELEMETRY_INTERVAL
    ) {
        lastTelemetryMillis = millis();

        mqttPublishTelemetry(
            dhtData,
            soilData,
            rainData,
            flowData,
            waterLevelData,
            rtcData
        );
    }


    // =================================================
    // 10. LCD
    // =================================================

    if (schedulerLCD())
    {
        if (
            dhtData.valid &&
            soilData.valid &&
            rainData.valid &&
            flowData.valid &&
            waterLevelData.valid
        )
        {
            lcdUpdate(

                // DHT
                dhtData.temperature,
                dhtData.humidity,

                // SOIL
                soilData.moisturePercent,

                // RAIN
                rainData.rainPercent,

                // FLOW
                flowData.flowRateLMin,
                flowData.totalLitres,

                // WATER LEVEL
                waterLevelData.waterLevelPercent,

                // IRRIGATION SESSION
                irrigationGetSessionLitres(),
                irrigationGetSessionCost(),

                // RELAY
                relayIsOn(),

                // IRRIGATION
                irrigationIsActive(),

                // RTC
                rtcData.valid,
                rtcData.hour,
                rtcData.minute,
                rtcData.second
            );
        }
    }


    // =================================================
    // 11. SERIAL MONITOR
    // =================================================

    if (schedulerSerial())
    {
        Serial.println();

        Serial.println(
            "========================================"
        );


        // =================================================
        // RTC
        // =================================================

        Serial.println(
            "--------------- RTC ------------------"
        );

        if (rtcData.valid)
        {
            Serial.print("[RTC] Date : ");

            Serial.print(rtcData.year);

            Serial.print("-");

            if (rtcData.month < 10)
            {
                Serial.print("0");
            }

            Serial.print(rtcData.month);

            Serial.print("-");

            if (rtcData.day < 10)
            {
                Serial.print("0");
            }

            Serial.println(rtcData.day);


            Serial.print("[RTC] Time : ");

            if (rtcData.hour < 10)
            {
                Serial.print("0");
            }

            Serial.print(rtcData.hour);

            Serial.print(":");

            if (rtcData.minute < 10)
            {
                Serial.print("0");
            }

            Serial.print(rtcData.minute);

            Serial.print(":");

            if (rtcData.second < 10)
            {
                Serial.print("0");
            }

            Serial.println(rtcData.second);


            Serial.print("[RTC] Timestamp : ");

            Serial.println(
                rtcGetTimestamp()
            );
        }
        else
        {
            Serial.println(
                "[RTC] Data invalid"
            );
        }


        // =================================================
        // DHT11
        // =================================================

        Serial.println(
            "--------------- DHT11 ----------------"
        );

        if (dhtData.valid)
        {
            Serial.print(
                "[DHT] Temperature : "
            );

            Serial.print(
                dhtData.temperature,
                1
            );

            Serial.println(" C");


            Serial.print(
                "[DHT] Humidity    : "
            );

            Serial.print(
                dhtData.humidity,
                1
            );

            Serial.println(" %");
        }
        else
        {
            Serial.println(
                "[DHT] Data invalid"
            );
        }


        // =================================================
        // SOIL
        // =================================================

        Serial.println(
            "------------ SOIL MOISTURE ------------"
        );

        if (soilData.valid)
        {
            Serial.print(
                "[SOIL] Raw ADC    : "
            );

            Serial.println(
                soilData.rawValue
            );


            Serial.print(
                "[SOIL] Moisture   : "
            );

            Serial.print(
                soilData.moisturePercent,
                1
            );

            Serial.println(" %");
        }
        else
        {
            Serial.println(
                "[SOIL] Data invalid"
            );
        }


        // =================================================
        // RAIN
        // =================================================

        Serial.println(
            "--------------- RAIN ------------------"
        );

        if (rainData.valid)
        {
            Serial.print(
                "[RAIN] Raw ADC    : "
            );

            Serial.println(
                rainData.rawValue
            );


            Serial.print(
                "[RAIN] Rain Level : "
            );

            Serial.print(
                rainData.rainPercent,
                1
            );

            Serial.println(" %");


            Serial.print(
                "[RAIN] Status     : "
            );

            if (rainData.raining)
            {
                Serial.println(
                    "RAINING"
                );
            }
            else
            {
                Serial.println(
                    "NO RAIN"
                );
            }
        }
        else
        {
            Serial.println(
                "[RAIN] Data invalid"
            );
        }


        // =================================================
        // FLOW
        // =================================================

        Serial.println(
            "--------------- FLOW ------------------"
        );

        if (flowData.valid)
        {
            Serial.print(
                "[FLOW] Flow Rate       : "
            );

            Serial.print(
                flowData.flowRateLMin,
                2
            );

            Serial.println(
                " L/min"
            );


            Serial.print(
                "[FLOW] Total Usage     : "
            );

            Serial.print(
                flowData.totalLitres,
                2
            );

            Serial.println(" L");


            Serial.print(
                "[FLOW] Total Volume    : "
            );

            Serial.print(
                flowData.totalCubicMeter,
                4
            );

            Serial.println(
                " m3"
            );


            Serial.print(
                "[FLOW] Total Cost      : "
            );

            Serial.println(
                flowData.totalCost,
                2
            );


            Serial.print(
                "[FLOW] Session Usage   : "
            );

            Serial.print(
                irrigationGetSessionLitres(),
                2
            );

            Serial.println(
                " L"
            );


            Serial.print(
                "[FLOW] Session Cost    : "
            );

            Serial.println(
                irrigationGetSessionCost(),
                2
            );
        }
        else
        {
            Serial.println(
                "[FLOW] Data invalid"
            );
        }


        // =================================================
        // WATER LEVEL
        // =================================================

        Serial.println(
            "------------- WATER LEVEL -------------"
        );

        if (waterLevelData.valid)
        {
            Serial.print(
                "[JSN] Distance    : "
            );

            Serial.print(
                waterLevelData.distanceCm,
                2
            );

            Serial.println(
                " cm"
            );


            Serial.print(
                "[JSN] Water Level : "
            );

            Serial.print(
                waterLevelData.waterLevelPercent,
                1
            );

            Serial.println(
                " %"
            );
        }
        else
        {
            Serial.println(
                "[JSN] Data invalid"
            );
        }


        // =================================================
        // RELAY
        // =================================================

        Serial.println(
            "--------------- RELAY -----------------"
        );

        Serial.print(
            "[RELAY] State       : "
        );

        if (relayIsOn())
        {
            Serial.println("ON");
        }
        else
        {
            Serial.println("OFF");
        }


        // =================================================
        // IRRIGATION
        // =================================================

        Serial.println(
            "------------- IRRIGATION --------------"
        );

        Serial.print(
            "[IRRIGATION] Active       : "
        );

        if (irrigationIsActive())
        {
            Serial.println("YES");
        }
        else
        {
            Serial.println("NO");
        }


        Serial.print(
            "[IRRIGATION] Session L    : "
        );

        Serial.println(
            irrigationGetSessionLitres(),
            2
        );


        Serial.print(
            "[IRRIGATION] Session Cost : "
        );

        Serial.println(
            irrigationGetSessionCost(),
            2
        );


        // =================================================
        // WIFI / MQTT STATUS
        // =================================================

        Serial.println(
            "------------- COMMUNICATION ------------"
        );

        Serial.print(
            "[WIFI] Status : "
        );

        if (wifiIsConnected())
        {
            Serial.println("CONNECTED");

            Serial.print(
                "[WIFI] SSID   : "
            );

            Serial.println(
                WiFi.SSID()
            );

            Serial.print(
                "[WIFI] IP     : "
            );

            Serial.println(
                WiFi.localIP()
            );

            Serial.print(
                "[WIFI] RSSI   : "
            );

            Serial.print(
                WiFi.RSSI()
            );

            Serial.println(" dBm");
        }
        else
        {
            Serial.println(
                "DISCONNECTED"
            );
        }


        Serial.print(
            "[MQTT] Status : "
        );

        if (mqttIsConnected())
        {
            Serial.println(
                "CONNECTED"
            );
        }
        else
        {
            Serial.println(
                "DISCONNECTED"
            );
        }


        // =================================================
        // SYSTEM PRINCIPLE
        // =================================================

        Serial.println();

        Serial.println(
            "[SYSTEM] Local control is independent from MQTT."
        );

        Serial.println(
            "[SYSTEM] Irrigation can continue without internet."
        );


        Serial.println(
            "----------------------------------------"
        );

        Serial.println();
    }
}