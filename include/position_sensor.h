#ifndef POSITION_SENSOR_H
#define POSITION_SENSOR_H

#include <stdint.h>

// ============================================================
//  POZISYON SENSORU (ESP32)
//   - Default: potansiyometre, ADC1 (12-bit)
//   - #define USE_ENCODER ile quadrature encoder (PCNT veya interrupt)
//
//  Aci konvansiyonu: -90 = Dogu, 0 = Zenit, +90 = Bati
// ============================================================

// #define USE_ENCODER

class PositionSensor {
public:
    void begin();

    // Aci derece (-90..+90)
    double readAngle();

    // Encoder kullaniliyorsa kalibrasyon icin tick setleme
    void setEncoderTicks(long ticks);
    long encoderTicks() const;

#ifdef USE_ENCODER
    static void IRAM_ATTR onEncoderA();
    static void IRAM_ATTR onEncoderB();
#endif

private:
    double last_angle_deg_ = 0.0;
};

struct LimitState {
    bool east;
    bool west;
};
LimitState readLimits();

#endif // POSITION_SENSOR_H
