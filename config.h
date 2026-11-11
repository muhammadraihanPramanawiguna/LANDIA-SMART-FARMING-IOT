#ifndef LANDIA_CONFIG_H
#define LANDIA_CONFIG_H

// =====================================================
// PIN CONFIGURATION
// =====================================================

// DHT11
#define DHT_PIN 4

// Soil Moisture V2
#define SOIL_PIN 34

// Rain Sensor
#define RAIN_PIN 35

// Water Flow ZJ-S201
#define FLOW_PIN 27

// JSN-SR04M
#define JSN_TRIG_PIN 5
#define JSN_ECHO_PIN 18

// Relay
#define RELAY_PIN 26

// =====================================================
// RELAY CONFIGURATION
// =====================================================

#define RELAY_ACTIVE_LOW true

// =====================================================
// I2C
// =====================================================

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// =====================================================
// LCD
// =====================================================

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// =====================================================
// SERIAL
// =====================================================

#define SERIAL_BAUDRATE 115200

// =====================================================
// WATER LEVEL CONFIGURATION
// =====================================================
//
// Jarak diukur dari sensor JSN ke permukaan air.
//
// <= FULL_DISTANCE  -> 100%
// FULL - EMPTY      -> linear
// >= EMPTY_DISTANCE -> 0%
//
// Nilai ini mengikuti konfigurasi yang sekarang kita gunakan.
// Tetap perlu divalidasi ketika instalasi final dilakukan.
//

#define WATER_LEVEL_FULL_DISTANCE_CM 30.0f
#define WATER_LEVEL_EMPTY_DISTANCE_CM 150.0f

// =====================================================
// IRRIGATION SCHEDULE
// =====================================================

// JADWAL TEST MALAM SEMENTARA
#define IRRIGATION_SCHEDULE_HOUR       21
#define IRRIGATION_SCHEDULE_MINUTE     24
#define IRRIGATION_MAX_DELAY_MINUTES   10
// Setelah jadwal utama, sistem masih boleh menunggu
// sampai batas waktu berikut.
//
// Contoh:
// 09:00 + 60 menit = 10:00

#define IRRIGATION_MAX_DELAY_MINUTES   60

// =====================================================
// SOIL MOISTURE TARGET
// =====================================================
//
// Penyiraman dilakukan selama:
// soil < STOP_PERCENT
//
// Penyiraman berhenti ketika:
// soil >= STOP_PERCENT
//
// START_PERCENT disimpan sebagai parameter informasi
// / threshold bawah, tetapi keputusan akhir berhenti
// tetap berdasarkan STOP_PERCENT.

#define SOIL_IRRIGATION_START_PERCENT  40.0f
#define SOIL_IRRIGATION_STOP_PERCENT   70.0f

// =====================================================
// WATER TANK SAFETY
// =====================================================
//
// Jika water level <= nilai ini,
// pompa tidak boleh berjalan.

#define WATER_LEVEL_MIN_IRRIGATION_PERCENT 20.0f

// =====================================================
// PUMP SAFETY
// =====================================================
//
// Maximum runtime satu sesi penyiraman.
//
// 300000 ms = 5 menit.

#define IRRIGATION_MAX_RUNTIME_MS 300000UL

// =====================================================
// FLOW SAFETY
// =====================================================
//
// Setelah pompa ON, flow sensor diberi waktu untuk
// mulai mendeteksi aliran.

#define FLOW_STARTUP_GRACE_MS 5000UL

// Flow minimal yang dianggap sebagai aliran air.

#define FLOW_MIN_DETECTED_LPM 0.10f

// =====================================================
// FLOW SENSOR CALIBRATION
// =====================================================
//
// Faktor sementara dari sensor yang sekarang.
// Nilai ini HARUS dikalibrasi menggunakan pengukuran
// volume air nyata sebelum sistem digunakan penuh.

#define FLOW_CALIBRATION_FACTOR 8.0f

// =====================================================
// WATER COST
// =====================================================
//
// Masukkan tarif air per liter setelah tarif resmi
// penggunaan air Landia Farm diketahui.
//
// 0.0 = biaya belum dikonfigurasi.

#define WATER_COST_PER_LITER 0.0f

#endif