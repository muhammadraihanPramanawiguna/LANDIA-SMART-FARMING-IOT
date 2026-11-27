
#include "lcd_manager.h"

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"


// =====================================================
// LCD OBJECT
// =====================================================

static LiquidCrystal_I2C lcd(
    LCD_ADDRESS,
    LCD_COLUMNS,
    LCD_ROWS
);


// =====================================================
// LCD CONFIGURATION
// =====================================================

static const uint8_t LCD_PAGE_COUNT = 3;
static const uint8_t LCD_LINE_LENGTH = 16;


// =====================================================
// PAGE STATE
// =====================================================

static uint8_t currentPage = 0;

static bool lcdInitialized = false;


// =====================================================
// LCD LINE CACHE
// =====================================================

// Cache menyimpan tampilan terakhir pada masing-masing baris.
// Tujuannya:
// - Menghindari penulisan LCD berulang jika teks tidak berubah.
// - Mengurangi flicker.
// - Tetap menulis seluruh 16 karakter ketika baris berubah.

static char lcdLineCache[LCD_ROWS][LCD_LINE_LENGTH + 1];

static bool lcdLineCacheValid[LCD_ROWS];


// =====================================================
// HELPER: VALIDATE FLOAT
// =====================================================

static bool lcdIsValidNumber(float value) {

    return isfinite(value);
}


// =====================================================
// HELPER: FORMAT FLOAT
// =====================================================

static void lcdFormatFloat(
    char* output,
    size_t outputSize,
    float value,
    uint8_t decimals
) {

    if (
        output == nullptr ||
        outputSize == 0
    ) {
        return;
    }

    if (!lcdIsValidNumber(value)) {

        snprintf(
            output,
            outputSize,
            "--"
        );

        return;
    }

    if (decimals == 0) {

        snprintf(
            output,
            outputSize,
            "%.0f",
            value
        );

    } else if (decimals == 1) {

        snprintf(
            output,
            outputSize,
            "%.1f",
            value
        );

    } else {

        snprintf(
            output,
            outputSize,
            "%.2f",
            value
        );
    }
}


// =====================================================
// HELPER: PRINT FIXED LCD LINE
// =====================================================

// Setiap baris:
// 1. Diisi spasi sebanyak 16 karakter.
// 2. Konten dimasukkan tanpa NULL-padding.
// 3. Dibandingkan dengan cache.
// 4. Jika berubah, seluruh 16 karakter ditulis.
//
// Dengan cara ini karakter dari halaman sebelumnya
// tidak akan tertinggal.

static void lcdPrintLine(
    uint8_t row,
    const char* content
) {

    if (
        !lcdInitialized ||
        row >= LCD_ROWS
    ) {
        return;
    }


    char buffer[LCD_LINE_LENGTH + 1];


    // =================================================
    // FILL BUFFER WITH SPACES
    // =================================================

    memset(
        buffer,
        ' ',
        LCD_LINE_LENGTH
    );

    buffer[LCD_LINE_LENGTH] = '\0';


    // =================================================
    // COPY CONTENT WITHOUT NULL PADDING
    // =================================================

    if (content != nullptr) {

        size_t contentLength = strlen(content);

        if (contentLength > LCD_LINE_LENGTH) {

            contentLength = LCD_LINE_LENGTH;
        }

        memcpy(
            buffer,
            content,
            contentLength
        );
    }


    // =================================================
    // CHECK CACHE
    // =================================================

    if (
        lcdLineCacheValid[row] &&
        memcmp(
            buffer,
            lcdLineCache[row],
            LCD_LINE_LENGTH
        ) == 0
    ) {

        // Tidak ada perubahan.
        // Tidak perlu menulis ulang LCD.

        return;
    }


    // =================================================
    // UPDATE CACHE
    // =================================================

    memcpy(
        lcdLineCache[row],
        buffer,
        LCD_LINE_LENGTH
    );

    lcdLineCache[row][LCD_LINE_LENGTH] = '\0';

    lcdLineCacheValid[row] = true;


    // =================================================
    // WRITE EXACTLY 16 CHARACTERS
    // =================================================

    lcd.setCursor(
        0,
        row
    );

    for (
        uint8_t column = 0;
        column < LCD_LINE_LENGTH;
        column++
    ) {

        lcd.write(
            static_cast<uint8_t>(buffer[column])
        );
    }
}


// =====================================================
// HELPER: PRINT TIME
// =====================================================

static void lcdFormatTime(
    char* output,
    size_t outputSize,
    int hour,
    int minute,
    int second
) {

    if (
        output == nullptr ||
        outputSize == 0
    ) {
        return;
    }

    if (
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59
    ) {

        snprintf(
            output,
            outputSize,
            "--:--:--"
        );

        return;
    }

    snprintf(
        output,
        outputSize,
        "%02d:%02d:%02d",
        hour,
        minute,
        second
    );
}


// =====================================================
// HELPER: RESET LCD CACHE
// =====================================================

static void lcdResetCache() {

    for (
        uint8_t row = 0;
        row < LCD_ROWS;
        row++
    ) {

        memset(
            lcdLineCache[row],
            ' ',
            LCD_LINE_LENGTH
        );

        lcdLineCache[row][LCD_LINE_LENGTH] = '\0';

        lcdLineCacheValid[row] = false;
    }
}


// =====================================================
// INITIALIZATION
// =====================================================

void lcdInit() {

    lcdInitialized = false;

    currentPage = 0;

    lcdResetCache();


    // =================================================
    // I2C
    // =================================================

    // Wire.begin() tidak dipanggil kembali di sini.
    //
    // RTC sudah menginisialisasi bus I2C.
    //
    // LCD dan RTC menggunakan bus yang sama.


    // =================================================
    // INITIALIZE LCD
    // =================================================

    lcd.init();

    lcd.backlight();

    lcd.clear();


    lcdInitialized = true;


    // =================================================
    // STARTUP DISPLAY
    // =================================================

    lcdPrintLine(
        0,
        "LANDIA FARM"
    );

    lcdPrintLine(
        1,
        "SYSTEM START..."
    );


    delay(1500);


    // =================================================
    // CLEAR DISPLAY
    // =================================================

    lcd.clear();

    lcdResetCache();


    // =================================================
    // RESET PAGE
    // =================================================

    currentPage = 0;


    // =================================================
    // SERIAL
    // =================================================

    Serial.println(
        "[LCD] LCD initialized"
    );

    Serial.println(
        "[LCD] 16-character line refresh enabled"
    );

    Serial.println(
        "[LCD] Line cache enabled"
    );

    Serial.println(
        "[LCD] I2C bus shared with RTC"
    );
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

    char temperatureText[8];
    char humidityText[8];

    char soilText[8];
    char rainText[8];

    char line1[32];
    char line2[32];


    // =================================================
    // FORMAT SENSOR DATA
    // =================================================

    lcdFormatFloat(
        temperatureText,
        sizeof(temperatureText),
        temperature,
        1
    );

    lcdFormatFloat(
        humidityText,
        sizeof(humidityText),
        humidity,
        0
    );

    lcdFormatFloat(
        soilText,
        sizeof(soilText),
        soilPercent,
        0
    );

    lcdFormatFloat(
        rainText,
        sizeof(rainText),
        rainPercent,
        0
    );


    // =================================================
    // LINE 1
    // =================================================

    snprintf(
        line1,
        sizeof(line1),
        "T:%sC H:%s%%",
        temperatureText,
        humidityText
    );


    // =================================================
    // LINE 2
    // =================================================

    snprintf(
        line2,
        sizeof(line2),
        "S:%s%% R:%s%%",
        soilText,
        rainText
    );


    // =================================================
    // DISPLAY
    // =================================================

    lcdPrintLine(
        0,
        line1
    );

    lcdPrintLine(
        1,
        line2
    );
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

    char flowText[8];
    char waterText[8];

    char sessionText[8];
    char totalText[8];
    char costText[8];

    char line1[32];
    char line2[32];


    // =================================================
    // FORMAT SENSOR DATA
    // =================================================

    lcdFormatFloat(
        flowText,
        sizeof(flowText),
        flowRate,
        1
    );

    lcdFormatFloat(
        waterText,
        sizeof(waterText),
        waterLevelPercent,
        0
    );

    lcdFormatFloat(
        sessionText,
        sizeof(sessionText),
        sessionLitres,
        1
    );

    lcdFormatFloat(
        totalText,
        sizeof(totalText),
        totalLitres,
        1
    );

    lcdFormatFloat(
        costText,
        sizeof(costText),
        sessionCost,
        0
    );


    // =================================================
    // LINE 1
    // =================================================

    snprintf(
        line1,
        sizeof(line1),
        "F:%sL/m W:%s%%",
        flowText,
        waterText
    );


    // =================================================
    // LINE 2
    // =================================================

    if (WATER_TARIFF_PER_M3 > 0.0f) {

        snprintf(
            line2,
            sizeof(line2),
            "U:%sL C:%s",
            sessionText,
            costText
        );

    } else {

        snprintf(
            line2,
            sizeof(line2),
            "U:%sL T:%sL",
            sessionText,
            totalText
        );
    }


    // =================================================
    // DISPLAY
    // =================================================

    lcdPrintLine(
        0,
        line1
    );

    lcdPrintLine(
        1,
        line2
    );
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

    char line1[32];
    char line2[32];

    char timeText[9];


    // =================================================
    // LINE 1
    // =================================================

    snprintf(
        line1,
        sizeof(line1),
        "PUMP:%s IRR:%s",
        pumpOn ? "ON" : "OFF",
        irrigationActive ? "ON" : "OFF"
    );


    // =================================================
    // LINE 2
    // =================================================

    if (rtcValid) {

        lcdFormatTime(
            timeText,
            sizeof(timeText),
            hour,
            minute,
            second
        );

        snprintf(
            line2,
            sizeof(line2),
            "%s RTC",
            timeText
        );

    } else {

        snprintf(
            line2,
            sizeof(line2),
            "RTC INVALID"
        );
    }


    // =================================================
    // DISPLAY
    // =================================================

    lcdPrintLine(
        0,
        line1
    );

    lcdPrintLine(
        1,
        line2
    );
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

    if (!lcdInitialized) {

        return;
    }


    // =================================================
    // PAGE 1: ENVIRONMENT
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
    // PAGE 2: WATER
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
    // PAGE 3: SYSTEM
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

    if (currentPage >= LCD_PAGE_COUNT) {

        currentPage = 0;
    }
}
