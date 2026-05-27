#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <stdint.h>

// ============================================================
//  SICAKLIK SENSORU - SOYUT ARAYUZ
//
//  Defacto donanim olarak MAX6675 K-tipi termokupl (SPI) kullaniyoruz.
//  Sensor degisirse sadece bu sinifin implementasyonunu degistir,
//  geri kalan kodu (tracker, main) ellemeye gerek yok.
//
//  read() okuyabilirse true doner ve out_celsius'u doldurur.
//  Sensor aciksa, hatali ise veya termokupl bagli degilse false doner.
// ============================================================

class TempSensor {
public:
    virtual ~TempSensor() {}
    virtual void begin() = 0;
    virtual bool read(float& out_celsius) = 0;
};

// --- MAX6675 (K-tipi termokupl, SPI, 0-1024 C, 0.25 C cozunurluk) ---
// SPI pinleri config.h: PIN_TEMP_CS, PIN_TEMP_SCK, PIN_TEMP_MISO
class TempSensorMax6675 : public TempSensor {
public:
    void begin() override;
    bool read(float& out_celsius) override;
};

// --- STUB (donanimsiz test icin sabit deger) ---
class TempSensorStub : public TempSensor {
public:
    explicit TempSensorStub(float value_c = 25.0f) : value_(value_c) {}
    void begin() override {}
    bool read(float& out_celsius) override { out_celsius = value_; return true; }
    void setValue(float c) { value_ = c; }
private:
    float value_;
};

#endif // TEMP_SENSOR_H
