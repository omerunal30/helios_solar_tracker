#include "position_sensor.h"
#include "config.h"
#include <Arduino.h>

#ifdef USE_ENCODER
static volatile long g_enc_ticks  = 0;
static volatile uint8_t g_enc_last_a = 0;
#endif

void PositionSensor::begin() {
    pinMode(PIN_LIMIT_EAST, INPUT_PULLUP);
    pinMode(PIN_LIMIT_WEST, INPUT_PULLUP);

#ifdef USE_ENCODER
    pinMode(PIN_ENC_A, INPUT);  // ESP32 GPIO 35/39 pull-up'i yok, harici PU gerek
    pinMode(PIN_ENC_B, INPUT);
    g_enc_last_a = digitalRead(PIN_ENC_A);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), &PositionSensor::onEncoderA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), &PositionSensor::onEncoderB, CHANGE);
#else
    // ESP32 ADC: ADC1 (GPIO 32-39) WiFi ile cakismaz
    analogReadResolution(12);             // 0..4095
    // analogSetAttenuation(ADC_11db);     // 0..3.3V tam aralik (default)
    pinMode(PIN_POT, INPUT);
#endif
}

double PositionSensor::readAngle() {
#ifdef USE_ENCODER
    long ticks;
    noInterrupts();
    ticks = g_enc_ticks;
    interrupts();
    last_angle_deg_ = ticks * DEGREES_PER_ENCODER_TICK;
#else
    // ESP32 ADC biraz gurultulu; 8 ornek ortalamasi
    long acc = 0;
    for (int i = 0; i < 8; ++i) acc += analogRead(PIN_POT);
    double raw = acc / 8.0;
    double t = raw / (double)ADC_MAX_COUNT;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    last_angle_deg_ = POT_ANGLE_MIN + t * (POT_ANGLE_MAX - POT_ANGLE_MIN);
#endif
    return last_angle_deg_;
}

void PositionSensor::setEncoderTicks(long ticks) {
#ifdef USE_ENCODER
    noInterrupts();
    g_enc_ticks = ticks;
    interrupts();
#else
    (void)ticks;
#endif
}

long PositionSensor::encoderTicks() const {
#ifdef USE_ENCODER
    long t;
    noInterrupts();
    t = g_enc_ticks;
    interrupts();
    return t;
#else
    return 0;
#endif
}

#ifdef USE_ENCODER
void IRAM_ATTR PositionSensor::onEncoderA() {
    uint8_t a = digitalRead(PIN_ENC_A);
    uint8_t b = digitalRead(PIN_ENC_B);
    if (a != g_enc_last_a) {
        if (a == b) g_enc_ticks++;
        else        g_enc_ticks--;
        g_enc_last_a = a;
    }
}
void IRAM_ATTR PositionSensor::onEncoderB() { /* opsiyonel */ }
#endif

LimitState readLimits() {
    LimitState s;
    s.east = (digitalRead(PIN_LIMIT_EAST) == LOW);
    s.west = (digitalRead(PIN_LIMIT_WEST) == LOW);
    return s;
}
