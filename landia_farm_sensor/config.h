
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
// GPIO5 merupakan ESP32 strapping pin.
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

// Jarak sensor ke permukaan air (cm).
// Jarak <= FULL_DISTANCE  -> 100%
// Jarak >= EMPTY_DISTANCE -> 0%

#define WATER_LEVEL_FULL_DISTANCE_CM 30.0f
#define WATER_LEVEL_EMPTY_DISTANCE_CM 150.0f

// =====================================================
// IRRIGATION SCHEDULE
// =====================================================

// Jadwal penyiraman menggunakan RTC lokal.
#define IRRIGATION_SCHEDULE_HOUR 9
#define IRRIGATION_SCHEDULE_MINUTE 0
#define IRRIGATION_MAX_DELAY_MINUTES 10

// =====================================================
// SOIL MOISTURE CONFIGURATION
// =====================================================

#define SOIL_MOISTURE_DRY_THRESHOLD_PERCENT 30.0f

#define SOIL_IRRIGATION_START_PERCENT 85.0f
#define SOIL_IRRIGATION_STOP_PERCENT 85.0f

// =====================================================
// WATER TANK SAFETY
// =====================================================

// Irigasi tidak boleh berjalan jika level air
// berada di bawah atau sama dengan batas ini.
#define WATER_LEVEL_MIN_IRRIGATION_PERCENT 20.0f

// =====================================================
// PUMP SAFETY
// =====================================================

// Durasi maksimum satu sesi irigasi: 5 menit.
#define IRRIGATION_MAX_RUNTIME_MS 300000UL

// =====================================================
// FLOW SENSOR SAFETY
// =====================================================

// Waktu toleransi setelah pompa ON.
#define FLOW_STARTUP_GRACE_MS 5000UL

// Aliran minimum yang dianggap terdeteksi.
#define FLOW_MIN_DETECTED_LPM 0.10f

// =====================================================
// FLOW SENSOR CALIBRATION
// =====================================================

// Nilai awal kalibrasi ZJ-S201.
// Satuan: pulsa per liter.
// WAJIB divalidasi menggunakan pengukuran volume aktual.
#define FLOW_CALIBRATION_FACTOR 450.0f

// =====================================================
// WATER COST CONFIGURATION
// =====================================================

// Tarif air dalam Rupiah per meter kubik (m3).
// 0.0 = tarif belum dikonfigurasi.
// Ubah sesuai tarif aktual Landia Farm.

#define WATER_TARIFF_PER_M3 0.0f

// Konversi satuan volume.
#define LITERS_PER_CUBIC_METER 1000.0f

// =====================================================
// TELEMETRY CONFIGURATION
// =====================================================

// Interval maksimum pembaruan telemetry.
// Perubahan signifikan dapat dikirim lebih cepat.
#define TELEMETRY_MAX_INTERVAL_MS 300000UL

// Interval pembacaan sensor tetap diatur
// terpisah dari interval pengiriman telemetry.

// =====================================================
// OFFLINE STORAGE CONFIGURATION
// =====================================================

// Interval penyimpanan telemetry offline.
#define OFFLINE_TELEMETRY_INTERVAL_MS 300000UL

// Interval pengiriman ulang antrean offline.
#define BACKLOG_SEND_INTERVAL_MS 500UL

// Batas waktu menunggu konfirmasi pengiriman.
// Mekanisme ACK akan disesuaikan dengan ThingsBoard.
#define ACK_TIMEOUT_INTERVAL_MS 10000UL

// =====================================================
// FLOW PERSISTENCE CONFIGURATION
// =====================================================

// Interval checkpoint volume ke NVS.
// Tidak menulis flash setiap pulsa sensor.
#define FLOW_PERSIST_INTERVAL_MS 10000UL

// Checkpoint tambahan berdasarkan pertambahan volume.
#define FLOW_PERSIST_VOLUME_THRESHOLD_L 1.0f

#endif
