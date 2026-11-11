#include "flow_sensor.h"
#include "config.h"

// =====================================================
// INTERNAL VARIABLES
// =====================================================

volatile unsigned long pulseCount = 0;

unsigned long lastFlowMillis = 0;

float flowRateLMin = 0.0f;

float totalLitres = 0.0f;


// =====================================================
// INTERRUPT
// =====================================================

void IRAM_ATTR flowPulseCounter() {

    pulseCount++;
}


// =====================================================
// INITIALIZATION
// =====================================================

void flowInit() {

    pinMode(
        FLOW_PIN,
        INPUT_PULLUP
    );


    pulseCount = 0;

    lastFlowMillis = millis();

    flowRateLMin = 0.0f;

    totalLitres = 0.0f;


    attachInterrupt(
        digitalPinToInterrupt(FLOW_PIN),
        flowPulseCounter,
        FALLING
    );


    Serial.println(
        "[FLOW] Sensor initialized"
    );

    Serial.print(
        "[FLOW] Calibration factor: "
    );

    Serial.println(
        FLOW_CALIBRATION_FACTOR,
        2
    );
}


// =====================================================
// UPDATE FLOW
// =====================================================

void flowUpdate() {

    unsigned long currentMillis =
        millis();


    // Update setiap 1 detik

    if (
        currentMillis -
        lastFlowMillis <
        1000UL
    ) {

        return;
    }


    // -------------------------------------------------
    // READ PULSE
    // -------------------------------------------------

    noInterrupts();

    unsigned long pulses =
        pulseCount;

    pulseCount = 0;

    interrupts();


    // -------------------------------------------------
    // CALCULATE FLOW RATE
    // -------------------------------------------------

    flowRateLMin =
        (
            pulses * 60.0f
        )
        /
        FLOW_CALIBRATION_FACTOR;


    // -------------------------------------------------
    // CALCULATE VOLUME
    // -------------------------------------------------
    //
    // Karena update dilakukan setiap ±1 detik:
    //
    // L/min / 60 = L/sec
    //

    float litresThisSecond =
        flowRateLMin / 60.0f;


    totalLitres +=
        litresThisSecond;


    // -------------------------------------------------
    // UPDATE TIMER
    // -------------------------------------------------

    lastFlowMillis =
        currentMillis;
}


// =====================================================
// READ DATA
// =====================================================

FlowData flowRead() {

    FlowData data;


    data.pulseCount = 0;

    data.flowRateLMin =
        flowRateLMin;

    data.totalLitres =
        totalLitres;

    data.totalCubicMeter =
        totalLitres / 1000.0f;


    data.totalCost =
        totalLitres *
        WATER_COST_PER_LITER;


    data.valid = true;


    return data;
}


// =====================================================
// RESET TOTAL
// =====================================================

void flowResetTotal() {

    noInterrupts();

    pulseCount = 0;

    interrupts();


    flowRateLMin = 0.0f;

    totalLitres = 0.0f;

    lastFlowMillis =
        millis();


    Serial.println(
        "[FLOW] Total usage reset"
    );
}