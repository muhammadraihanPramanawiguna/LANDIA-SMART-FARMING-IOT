
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

// Penyiraman hanya boleh dimulai ketika jadwal tiba.
// Jadwal menggunakan waktu RTC lokal (WIB).

#define IRRIGATION_SCHEDULE_HOUR 9
#define IRRIGATION_SCHEDULE_MINUTE 0

// Toleransi waktu eksekusi jadwal.
// Controller harus memastikan jadwal tidak dieksekusi
// berulang kali dalam satu hari.
#define IRRIGATION_MAX_DELAY_MINUTES 10

// =====================================================
// SOIL MOISTURE CONFIGURATION
// =====================================================

// Batas indikator tanah kering.
// Nilai ini merupakan indikator kondisi tanah,
// bukan trigger penyiraman di luar jadwal.
#define SOIL_MOISTURE_DRY_THRESHOLD_PERCENT 30.0f

// =====================================================
// SCHEDULED IRRIGATION TARGET
// =====================================================

// Target kelembapan tanah.
//
// Logika yang diharapkan:
// 1. Tunggu jadwal penyiraman.
// 2. Ketika jadwal tiba, periksa soil moisture.
// 3. Jika moisture >= target, penyiraman dilewati.
// 4. Jika moisture < target, penyiraman dimulai.
// 5. Pompa berjalan hingga moisture mencapai target.
// 6. Setelah mencapai target, pompa OFF.
// 7. Tunggu jadwal penyiraman berikutnya.
//
// Moisture di bawah target TIDAK boleh menjadi trigger
// penyiraman sebelum jadwal tiba.

#define SOIL_IRRIGATION_START_PERCENT 30.0f
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
//
// Safety timeout untuk mencegah pompa bekerja
// terlalu lama apabila target kelembapan tidak tercapai.
#define IRRIGATION_MAX_RUNTIME_MS 300000UL

// =====================================================
// FLOW SENSOR SAFETY
// =====================================================

// Startup flow detection timeout:
// waktu maksimal mendeteksi aliran pertama
// setelah pompa dinyalakan.
#define FLOW_STARTUP_DETECTION_TIMEOUT_MS 12000UL

// Flow-loss timeout:
// jika flow yang sebelumnya terdeteksi hilang
// selama durasi ini, pompa akan dimatikan.
#define FLOW_LOSS_TIMEOUT_MS 5000UL

// Ambang flow untuk mengabaikan noise
// atau pembacaan aliran sangat kecil.
//
// Nilai ini perlu divalidasi pada sistem drip
// Landia Farm dengan pompa dan jalur aktual.
#define FLOW_MIN_DETECTED_LPM 0.30f

// =====================================================
// FLOW SENSOR CALIBRATION
// =====================================================

// Nilai awal kalibrasi ZJ-S201.
// Satuan: pulsa per liter.
//
// WAJIB divalidasi menggunakan volume air aktual.
#define FLOW_CALIBRATION_FACTOR 450.0f

// =====================================================
// WATER COST CONFIGURATION
// =====================================================

// Tarif air dalam Rupiah per meter kubik (m3).
// Nilai ini harus disesuaikan dengan tarif aktual.
//
// 0.0 = tarif belum dikonfigurasi.
#define WATER_TARIFF_PER_M3 1000.0f

// Konversi satuan volume.
#define LITERS_PER_CUBIC_METER 1000.0f

// =====================================================
// THINGSBOARD MQTT CONFIGURATION
// =====================================================

// Endpoint yang sudah diuji langsung
// pada server Landia Farm.
#define THINGSBOARD_MQTT_HOST "194.233.73.154"
#define THINGSBOARD_MQTT_PORT 1883
#define THINGSBOARD_MQTT_TOPIC "esp/telemetry"
#define THINGSBOARD_MQTT_QOS 1

// =====================================================
// WIFI & MQTT RECONNECT CONFIGURATION
// =====================================================

// Interval koneksi ulang Wi-Fi.
#define WIFI_RECONNECT_INTERVAL_MS 10000UL

// Interval koneksi ulang MQTT.
#define MQTT_RECONNECT_INTERVAL_MS 10000UL

// Interval pengiriman backlog MQTT.
#define MQTT_BACKLOG_SEND_INTERVAL_MS 500UL

// =====================================================
// TELEMETRY CONFIGURATION
// =====================================================

// Interval maksimum pembaruan telemetry.
// Perubahan signifikan dapat dikirim lebih cepat.
#define TELEMETRY_MAX_INTERVAL_MS 15000UL

// Interval pembacaan sensor diatur oleh scheduler
// dan tidak bergantung pada interval telemetry.

// =====================================================
// OFFLINE STORAGE CONFIGURATION
// =====================================================

// Interval penyimpanan telemetry offline.
#define OFFLINE_TELEMETRY_INTERVAL_MS 300000UL

// Interval pengiriman ulang antrean offline.
#define BACKLOG_SEND_INTERVAL_MS 500UL

// Batas waktu menunggu konfirmasi pengiriman.
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
