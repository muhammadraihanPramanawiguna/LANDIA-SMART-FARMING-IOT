
#ifndef LANDIA_CONFIG_H
#define LANDIA_CONFIG_H

// =====================================================
// PIN CONFIGURATION
// ESP32 LANDIA SMART FARMING
// =====================================================

// DHT11
#define DHT_PIN 4

// Capacitive Soil Moisture Sensor V2.0.0
#define SOIL_PIN 34

// Rain Sensor
#define RAIN_PIN 35

// Water Flow Sensor ZJ-S201
#define FLOW_PIN 27

// Ultrasonic Water Level Sensor JSN-SR04M
// GPIO5 adalah ESP32 strapping pin.
// Pastikan rangkaian tidak mengganggu proses boot.
#define JSN_TRIG_PIN 5
#define JSN_ECHO_PIN 18

// Relay Module
#define RELAY_PIN 26

// =====================================================
// RELAY CONFIGURATION
// =====================================================

// true  = Relay aktif ketika LOW
// false = Relay aktif ketika HIGH
#define RELAY_ACTIVE_LOW true

// =====================================================
// I2C CONFIGURATION
// =====================================================

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// =====================================================
// LCD CONFIGURATION
// =====================================================

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// =====================================================
// SERIAL CONFIGURATION
// =====================================================

#define SERIAL_BAUDRATE 115200

// =====================================================
// WATER LEVEL CONFIGURATION
// =====================================================
//
// Jarak diukur dari sensor JSN ke permukaan air.
//
// <= FULL_DISTANCE  -> 100%
// >= EMPTY_DISTANCE -> 0%
// Di antara keduanya -> interpolasi linear
//
// Nilai masih perlu divalidasi saat instalasi final.
//

#define WATER_LEVEL_FULL_DISTANCE_CM 30.0f
#define WATER_LEVEL_EMPTY_DISTANCE_CM 150.0f

// =====================================================
// IRRIGATION SCHEDULE
// =====================================================
//
// Jadwal penyiraman: 09:00 WIB.
// Toleransi keterlambatan jadwal: 10 menit.
//
// Jadwal tetap menggunakan RTC lokal WIB.
//

#define IRRIGATION_SCHEDULE_HOUR 9
#define IRRIGATION_SCHEDULE_MINUTE 0
#define IRRIGATION_MAX_DELAY_MINUTES 10

// =====================================================
// SOIL MOISTURE CONFIGURATION
// =====================================================

// Indikator tanah sangat kering.
// Tidak digunakan sebagai syarat mulai irigasi.

#define SOIL_MOISTURE_DRY_THRESHOLD_PERCENT 30.0f

// Target kelembapan ketika jadwal penyiraman tiba.
// Soil < 85%  -> penyiraman dimulai.
// Soil >= 85% -> penyiraman dilewati atau dihentikan.

#define SOIL_IRRIGATION_START_PERCENT 85.0f
#define SOIL_IRRIGATION_STOP_PERCENT 85.0f

// =====================================================
// WATER TANK SAFETY
// =====================================================
//
// Jika level air <= 20%,
// pompa tidak boleh menjalankan irigasi.
//

#define WATER_LEVEL_MIN_IRRIGATION_PERCENT 20.0f

// =====================================================
// PUMP SAFETY
// =====================================================
//
// Durasi maksimum satu sesi penyiraman.
// 300000 ms = 5 menit.
//

#define IRRIGATION_MAX_RUNTIME_MS 300000UL

// =====================================================
// FLOW SENSOR SAFETY
// =====================================================
//
// Toleransi setelah pompa ON sebelum flow
// sensor wajib mendeteksi aliran.
//

#define FLOW_STARTUP_GRACE_MS 5000UL

// Aliran minimum yang dianggap terdeteksi.
#define FLOW_MIN_DETECTED_LPM 0.10f

// =====================================================
// FLOW SENSOR CALIBRATION
// =====================================================
//
// Nilai awal untuk YF-S201 / ZJ-S201.
// Satuan: pulsa per liter.
// Wajib dikalibrasi menggunakan volume air aktual.
//

#define FLOW_CALIBRATION_FACTOR 450.0f

// =====================================================
// WATER COST CONFIGURATION
// =====================================================
//
// Tarif air per liter.
// 0.0 = tarif belum dikonfigurasi.
//

#define WATER_COST_PER_LITER 0.0f

#endif
