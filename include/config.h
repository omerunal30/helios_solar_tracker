#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
//  PARABOL OLUK GUNES TAKIP SISTEMI - KONFIGURASYON
//  Donanim:
//    - MCU:    ESP32 (DevKit)
//    - Servo:  Delta ASDA-B2 (Pulse + Direction modu, Pt)
//    - RTC:    DS3231 (I2C)
//    - Pozisyon: Potansiyometre (default) veya quadrature encoder
//    - Sicaklik: MAX6675 K-tipi termokupl (SPI) [default; degistirilebilir]
//  Optik: 6m genislik x 3 sira x 6m = 18m uzunluk, N-S yatay donme ekseni
//         (oluk 18m uzunlugu Kuzey-Guney yonlu dosenir, gunesi Dogu->Bati
//          takip etmek icin bu eksen etrafinda doner)
// ============================================================

// --- Konum (Ankara varsayilan) ---
constexpr double LATITUDE  = 39.9334;
constexpr double LONGITUDE = 32.8597;
constexpr int    UTC_OFFSET_HOURS = 3;

// --- Aci Limitleri (-90=Dogu, 0=Zenit, +90=Bati) ---
constexpr double TRACK_ANGLE_MIN = -90.0;
constexpr double TRACK_ANGLE_MAX =  90.0;
constexpr double HOME_ANGLE      = -90.0;   // Sabah baslangic (dogu)
constexpr double STOW_ANGLE      = -90.0;   // Gece park = ertesi gunun baslangici
                                            // (sabah HOME_FIND atlanir, panel zaten yerinde)
constexpr double DEFOCUS_PARK_ANGLE = 0.0;  // Asiri sicaklikta park (zenit) - gunes
                                            // hicbir acida odakta toplanmaz

// --- ASDA-B2 Servo / Mekanik ---
// Servo'nun bir tam devir icin gereken pulse (ASDA P1-44/P1-45 elektronik dislisi).
// ASDA fabrika varsayilani: 10000 pulse/devir.
constexpr long   PULSES_PER_MOTOR_REV = 10000;
// Motor-oluk arasi mekanik redaktor orani (saha kurulumuna gore ayarlanmalidir)
constexpr double GEAR_RATIO = 100.0;        // 100:1 (orn: solucan dislisi)
// Bir derece oluk dönüsü için gereken pulse sayisi:
constexpr double PULSES_PER_DEGREE =
    (double)PULSES_PER_MOTOR_REV * GEAR_RATIO / 360.0; // ~2778

// --- Pulse Frekansi (motor hizi) ---
// Hiz iliskisi: deg_per_sec = pulse_freq / PULSES_PER_DEGREE
// Gercekci parabol oluk slew rate'leri (SkyTrough, Eurotrough, LS-3): ~1 deg/s.
// Daha yuksek hiz mekanik strese ve servo overshoot'una yol acar.
constexpr int PULSE_FREQ_MIN  = 100;    // ~0.036 deg/s (cok yumusak baslangic)
constexpr int PULSE_FREQ_MAX  = 3000;   // ~1.08 deg/s (slew/cruise hizi)

// --- Rampalama Profili ---
constexpr unsigned long RAMP_UP_TIME_MS   = 2500;
constexpr unsigned long RAMP_DOWN_TIME_MS = 2500;
constexpr unsigned long RAMP_STEP_MS      = 50;

// --- ESP32 Pin Atamalari ---
// Motor sinyalleri (ASDA-B2 CN1)
// NOT: ASDA inputlari 5V veya 24V optoizoleli. ESP32 3.3V'tan -> seviye
// uydurucu (74HC125, 4N25 opto vs) gerekebilir. Aksi halde girisler tetiklenmez.
constexpr int PIN_PULSE   = 25;   // -> ASDA PULSE+ (CN1, PULSE girisi)
constexpr int PIN_DIR     = 26;   // -> ASDA SIGN+  (CN1, yon girisi)
constexpr int PIN_SERVO_ON = 27;  // -> ASDA SON    (CN1, servo on; optoizoleli)

// Limit switch'ler (INPUT_PULLUP, basili = LOW)
constexpr int PIN_LIMIT_EAST = 32;
constexpr int PIN_LIMIT_WEST = 33;
// NOT: Acil durum icin ASDA'nin kendi CWL/CCWL/EMGS girislerine de paralel
// baglanmasi tavsiye edilir (P2-15..P2-17 ile assign).

// Pozisyon: ADC1 (input-only) pinleri tercih edilmeli
constexpr int PIN_POT = 34;
constexpr double POT_ANGLE_MIN = -90.0;
constexpr double POT_ANGLE_MAX =  90.0;
constexpr int    ADC_MAX_COUNT = 4095;   // ESP32 ADC 12-bit

// Quadrature encoder (USE_ENCODER tanimliysa)
constexpr int PIN_ENC_A = 35;
constexpr int PIN_ENC_B = 39;
constexpr double DEGREES_PER_ENCODER_TICK = 0.05;

// Sicaklik sensoru (MAX6675 SPI)
constexpr int PIN_TEMP_CS   = 5;
constexpr int PIN_TEMP_SCK  = 18;   // ESP32 default VSPI SCK
constexpr int PIN_TEMP_MISO = 19;   // ESP32 default VSPI MISO

// I2C (DS3231): ESP32 default SDA=21, SCL=22 (Wire.begin() default)

// --- LEDC (pulse uretim) ---
constexpr int LEDC_CHANNEL    = 0;
constexpr int LEDC_RESOLUTION = 8;     // 8-bit duty (0-255)
constexpr int LEDC_DUTY_50    = 128;   // %50 duty (kare dalga icin)

// --- Sicaklik Esikleri (Celsius) - HISTEREZIS ---
constexpr float TEMP_DEFOCUS_HIGH = 350.0f;  // Bunu astiginda gunesten kac
constexpr float TEMP_RESUME_LOW   = 280.0f;  // Bunun altina dusunce takibe don
constexpr float TEMP_ERROR_MAX    = 600.0f;  // Sensor okumasi bunun ustunde -> ariza
constexpr unsigned long TEMP_READ_INTERVAL_MS = 1000;

// --- Tracker Davranisi ---
constexpr double ANGLE_TOLERANCE    = 0.3;
constexpr double ANGLE_RAMP_ZONE    = 3.0;
constexpr unsigned long TRACK_INTERVAL_MS = 10000;

// --- DS3231 ---
constexpr uint8_t RTC_I2C_ADDR = 0x68;

// --- Gunes Esikleri ---
constexpr double SUNRISE_MIN_ELEVATION = 2.0;
constexpr double SUNSET_MIN_ELEVATION  = 2.0;

// --- ASDA-B2 onerilen parametreler (sürücüde elle ayarlanir) ---
// P1-00  = 0x0002   (PUL + SIGN pulse formati)
// P1-01  = 0x0000   (Pt mode - external position command)
// P1-32  = 0x0000   (motor stop modu)
// P1-44  / P1-45    (elektronik dislisi - PULSES_PER_MOTOR_REV ile uyumlu)
// P2-15..P2-17      (DI baglantilarini etkin/devre disi)
// P2-22             (EMGS aktif, normalde kapali emergency stop)
// Detaylar ASDA-B2 kullanim kilavuzunda.

#endif // CONFIG_H
