#include "lcd_manager.h"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "config.h"


// =====================================================
// LCD OBJECT
// =====================================================

LiquidCrystal_I2C lcd(
    LCD_ADDRESS,
    LCD_COLUMNS,
    LCD_ROWS
);


// =====================================================
// PAGE
// =====================================================

static uint8_t currentPage = 0;


// =====================================================
// INITIALIZATION
// =====================================================

void lcdInit() {

    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    lcd.init();

    lcd.backlight();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("LANDIA FARM");

    lcd.setCursor(0, 1);
    lcd.print("SYSTEM START...");

    delay(1500);

    lcd.clear();

    Serial.println(
        "[LCD] LCD initialized"
    );
}


// =====================================================
// CLEAR LCD
// =====================================================

static void lcdClear() {

    lcd.clear();
}


// =====================================================
// PAGE 1
// ENVIRONMENT
// =====================================================

static void showEnvironmentPage(
    float temperature,
    float humidity,
    float soilPercent,
    float rainPercent
) {

    lcdClear();

    // LINE 1
    lcd.setCursor(0, 0);

    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C ");

    lcd.print("H:");
    lcd.print(humidity, 0);
    lcd.print("%");


    // LINE 2
    lcd.setCursor(0, 1);

    lcd.print("S:");
    lcd.print(soilPercent, 0);
    lcd.print("% ");

    lcd.print("R:");
    lcd.print(rainPercent, 0);
    lcd.print("%");
}


// =====================================================
// PAGE 2
// WATER
// =====================================================

static void showWaterPage(
    float flowRate,
    float totalLitres,
    float waterLevelPercent,
    float sessionLitres,
    float sessionCost
) {

    lcdClear();

    // -------------------------------------------------
    // LINE 1
    // -------------------------------------------------

    lcd.setCursor(0, 0);

    lcd.print("F:");
    lcd.print(flowRate, 1);
    lcd.print("L/m ");

    lcd.print("W:");
    lcd.print(waterLevelPercent, 0);
    lcd.print("%");


    // -------------------------------------------------
    // LINE 2
    // -------------------------------------------------

    lcd.setCursor(0, 1);

    lcd.print("U:");
    lcd.print(sessionLitres, 1);
    lcd.print("L");


    if (WATER_COST_PER_LITER > 0.0f) {

        lcd.print(" C:");
        lcd.print(sessionCost, 0);
    }

    else {

        lcd.print(" T:");
        lcd.print(totalLitres, 1);
        lcd.print("L");
    }
}


// =====================================================
// PAGE 3
// SYSTEM
// =====================================================

static void showSystemPage(
    bool pumpOn,
    bool irrigationActive,
    bool rtcValid,
    int hour,
    int minute,
    int second
) {

    lcdClear();

    // -------------------------------------------------
    // LINE 1
    // -------------------------------------------------

    lcd.setCursor(0, 0);

    lcd.print("PUMP:");

    if (pumpOn) {

        lcd.print("ON ");
    }

    else {

        lcd.print("OFF");
    }


    lcd.print(" IRR:");

    if (irrigationActive) {

        lcd.print("ON");
    }

    else {

        lcd.print("OFF");
    }


    // -------------------------------------------------
    // LINE 2
    // -------------------------------------------------

    lcd.setCursor(0, 1);

    if (rtcValid) {

        if (hour < 10) {
            lcd.print("0");
        }

        lcd.print(hour);
        lcd.print(":");


        if (minute < 10) {
            lcd.print("0");
        }

        lcd.print(minute);
        lcd.print(":");


        if (second < 10) {
            lcd.print("0");
        }

        lcd.print(second);

        lcd.print(" RTC");
    }

    else {

        lcd.print("RTC INVALID");
    }
}


// =====================================================
// MAIN LCD UPDATE
// =====================================================

void lcdUpdate(

    float temperature,
    float humidity,

    float soilPercent,
    float rainPercent,

    float flowRate,
    float totalLitres,

    float waterLevelPercent,

    float sessionLitres,
    float sessionCost,

    bool pumpOn,
    bool irrigationActive,

    bool rtcValid,

    int hour,
    int minute,
    int second
) {

    // =================================================
    // PAGE 1
    // =================================================

    if (currentPage == 0) {

        showEnvironmentPage(
            temperature,
            humidity,
            soilPercent,
            rainPercent
        );
    }


    // =================================================
    // PAGE 2
    // =================================================

    else if (currentPage == 1) {

        showWaterPage(
            flowRate,
            totalLitres,
            waterLevelPercent,
            sessionLitres,
            sessionCost
        );
    }


    // =================================================
    // PAGE 3
    // =================================================

    else if (currentPage == 2) {

        showSystemPage(
            pumpOn,
            irrigationActive,
            rtcValid,
            hour,
            minute,
            second
        );
    }


    // =================================================
    // NEXT PAGE
    // =================================================

    currentPage++;

    if (currentPage > 2) {

        currentPage = 0;
    }
}