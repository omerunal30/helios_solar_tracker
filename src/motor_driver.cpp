#include "motor_driver.h"
#include "config.h"
#include <Arduino.h>

// ============================================================
//  LEDC API: Arduino-ESP32 v2.x ve v3.x arasinda degisti.
//  Burada ESP_ARDUINO_VERSION_MAJOR ile her ikisi de desteklenir.
// ============================================================

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

static bool g_pulse_attached = false;

static void ledcStart(int pin, int initial_freq, int resolution) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(pin, initial_freq, resolution);
#else
    ledcSetup(LEDC_CHANNEL, initial_freq, resolution);
    ledcAttachPin(pin, LEDC_CHANNEL);
#endif
}

static void ledcSetFreq(int pin, int freq) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcChangeFrequency(pin, freq, LEDC_RESOLUTION);
    ledcWrite(pin, LEDC_DUTY_50);   // %50 duty = kare dalga
#else
    ledcChangeFrequency(LEDC_CHANNEL, freq, LEDC_RESOLUTION);
    ledcWrite(LEDC_CHANNEL, LEDC_DUTY_50);
#endif
}

static void ledcSilence(int pin) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(pin, 0);
#else
    (void)pin;
    ledcWrite(LEDC_CHANNEL, 0);
#endif
}

void MotorDriver::begin() {
    pinMode(PIN_DIR,       OUTPUT);
    pinMode(PIN_SERVO_ON,  OUTPUT);
    digitalWrite(PIN_DIR,      LOW);
    digitalWrite(PIN_SERVO_ON, LOW);

    // LEDC'yi 1 kHz dummy ile baslat (ledcSilence ile susturulur)
    ledcStart(PIN_PULSE, 1000, LEDC_RESOLUTION);
    ledcSilence(PIN_PULSE);
    g_pulse_attached = true;

    state_        = ST_IDLE;
    current_freq_ = 0;
    current_dir_  = DIR_STOP;
    servo_on_     = false;
}

int MotorDriver::effectiveTarget() const {
    int t = target_freq_;
    if (freq_cap_ >= 0 && t > freq_cap_) t = freq_cap_;
    if (t > 0 && t < PULSE_FREQ_MIN) t = PULSE_FREQ_MIN;
    if (t > PULSE_FREQ_MAX) t = PULSE_FREQ_MAX;
    return t;
}

void MotorDriver::setServoOn(bool on) {
    servo_on_ = on;
    digitalWrite(PIN_SERVO_ON, on ? HIGH : LOW);
}

void MotorDriver::setTarget(int target_freq_hz, Direction dir) {
    if (target_freq_hz <= 0 || dir == DIR_STOP) {
        stop();
        return;
    }
    if (target_freq_hz > PULSE_FREQ_MAX) target_freq_hz = PULSE_FREQ_MAX;

    // Zaten ayni yone reverse oluyorsak SADECE pending_freq guncelle, RESTART YAPMA.
    // Bu kontrol olmazsa, ust katman her loop iterasyonunda setTarget cagirdiginda
    // ramp_start_ms_ sifirlanir ve REVERSING asla tamamlanmaz (motor kilitlenir).
    if (state_ == ST_REVERSING && pending_dir_ == dir) {
        pending_freq_ = target_freq_hz;
        return;
    }

    // Yon degisikligi: once rampla dur, sonra yeni yonde basla
    if (current_dir_ != DIR_STOP && current_dir_ != dir) {
        pending_dir_  = dir;
        pending_freq_ = target_freq_hz;
        state_           = ST_REVERSING;
        ramp_start_ms_   = millis();
        ramp_start_freq_ = current_freq_;
        return;
    }

    target_freq_ = target_freq_hz;
    current_dir_ = dir;
    if (state_ == ST_IDLE || state_ == ST_RAMP_DOWN) {
        state_           = ST_RAMP_UP;
        ramp_start_ms_   = millis();
        ramp_start_freq_ = current_freq_;
    }
}

void MotorDriver::stop() {
    // Zaten duruyor/yavasliyorsa restart yapma (ust katman stop'u her loop'ta cagirabilir)
    if (state_ == ST_IDLE || state_ == ST_RAMP_DOWN) return;
    if (current_freq_ == 0) {
        state_       = ST_IDLE;
        current_dir_ = DIR_STOP;
        return;
    }
    state_           = ST_RAMP_DOWN;
    ramp_start_ms_   = millis();
    ramp_start_freq_ = current_freq_;
    pending_dir_     = DIR_STOP;
}

void MotorDriver::emergencyStop() {
    ledcSilence(PIN_PULSE);
    digitalWrite(PIN_SERVO_ON, LOW);
    servo_on_      = false;
    current_freq_  = 0;
    current_dir_   = DIR_STOP;
    state_         = ST_IDLE;
    pending_dir_   = DIR_STOP;
}

void MotorDriver::writePulseFreq(int hz) {
    if (hz <= 0) {
        ledcSilence(PIN_PULSE);
        return;
    }
    if (hz < 1)       hz = 1;
    if (hz > 1000000) hz = 1000000; // ESP32 LEDC pratikte cok yuksek frekansa cikar
    ledcSetFreq(PIN_PULSE, hz);
}

void MotorDriver::update() {
    unsigned long now = millis();
    if (now - last_step_ms_ < RAMP_STEP_MS) return;
    last_step_ms_ = now;

    switch (state_) {

    case ST_IDLE:
        current_freq_ = 0;
        break;

    case ST_RAMP_UP: {
        unsigned long elapsed = now - ramp_start_ms_;
        int tgt = effectiveTarget();
        if (elapsed >= RAMP_UP_TIME_MS) {
            current_freq_ = tgt;
            state_        = ST_CRUISE;
        } else {
            long delta = (long)tgt - (long)ramp_start_freq_;
            current_freq_ = ramp_start_freq_ + (int)((delta * (long)elapsed) / (long)RAMP_UP_TIME_MS);
            if (current_freq_ < PULSE_FREQ_MIN) current_freq_ = PULSE_FREQ_MIN;
            if (current_freq_ > tgt)            current_freq_ = tgt;
        }
        break;
    }

    case ST_CRUISE: {
        int tgt = effectiveTarget();
        if (current_freq_ < tgt) {
            // Cap kalkti veya hedef artti -> tekrar rampUp
            state_           = ST_RAMP_UP;
            ramp_start_ms_   = now;
            ramp_start_freq_ = current_freq_;
        } else if (current_freq_ > tgt) {
            // Cap dustu (acidan rampa) -> kucuk adimlarla dusur
            int step = (PULSE_FREQ_MAX - PULSE_FREQ_MIN) / 60; // ~60 adimda full duser
            if (step < 50) step = 50;
            current_freq_ -= step;
            if (current_freq_ < tgt) current_freq_ = tgt;
        }
        break;
    }

    case ST_RAMP_DOWN: {
        unsigned long elapsed = now - ramp_start_ms_;
        if (elapsed >= RAMP_DOWN_TIME_MS) {
            current_freq_ = 0;
            current_dir_  = DIR_STOP;
            state_        = ST_IDLE;
        } else {
            long delta = (long)ramp_start_freq_;
            current_freq_ = ramp_start_freq_ - (int)((delta * (long)elapsed) / (long)RAMP_DOWN_TIME_MS);
            if (current_freq_ < 0) current_freq_ = 0;
        }
        break;
    }

    case ST_REVERSING: {
        unsigned long elapsed = now - ramp_start_ms_;
        if (elapsed >= RAMP_DOWN_TIME_MS || current_freq_ <= 0) {
            current_freq_ = 0;
            current_dir_  = pending_dir_;
            target_freq_  = pending_freq_;
            state_           = ST_RAMP_UP;
            ramp_start_ms_   = now;
            ramp_start_freq_ = 0;
            pending_dir_     = DIR_STOP;
        } else {
            long delta = (long)ramp_start_freq_;
            current_freq_ = ramp_start_freq_ - (int)((delta * (long)elapsed) / (long)RAMP_DOWN_TIME_MS);
            if (current_freq_ < 0) current_freq_ = 0;
        }
        break;
    }
    }

    applyHardware();
}

void MotorDriver::applyHardware() {
    if (current_freq_ <= 0 || current_dir_ == DIR_STOP) {
        writePulseFreq(0);
        return;
    }
    if (!servo_on_) {
        // SON sinyali yoksa servo zaten hareket etmez; otomatik enable et
        setServoOn(true);
        delayMicroseconds(100);  // ASDA enable delay (pratikte daha uzun olabilir)
    }
    // DIR sinyali: WEST = HIGH, EAST = LOW (saha kalibrasyonu sirasinda
    // ters cikarsa polariteyi degistir veya ASDA P1-01 ile yon bayrak biti
    // ile ayarla)
    digitalWrite(PIN_DIR, (current_dir_ == DIR_WEST) ? HIGH : LOW);
    writePulseFreq(current_freq_);
}
