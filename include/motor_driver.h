#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>

// ============================================================
//  MOTOR SURUCUSU - DELTA ASDA-B2 (PULSE + DIRECTION)
//
//  ESP32 LEDC peripheral ile pulse uretimi (kare dalga, %50 duty,
//  degisken frekans). Frekans = motor hizi.
//
//  Trapez profil:
//    DURDU -> RAMP_UP (PULSE_FREQ_MIN'den hedef freq'e)
//    CRUISE (hedef frekansta)
//    RAMP_DOWN (hedef freq'ten 0'a, durur)
//  Yon degisirse: once tam durur, sonra ters yonde rampUp.
// ============================================================

class MotorDriver {
public:
    enum Direction { DIR_STOP = 0, DIR_EAST = -1, DIR_WEST = +1 };

    void begin();

    // Hedef pulse frekansi + yon. setTarget(0,...) = stop ile ayni.
    void setTarget(int target_freq_hz, Direction dir);

    void stop();           // rampalı dur
    void emergencyStop();  // pulse'ı kes, SON sinyalini düşür
    void setServoOn(bool on);

    // Periyodik olarak loop()'tan cagrilmali.
    void update();

    // Geçerli motor frekansi (Hz). 0 = duruyor.
    int  currentFreq() const { return current_freq_; }
    bool isMoving() const    { return current_freq_ > 0; }
    Direction currentDir() const { return current_dir_; }

    // Tracker, hedefe yaklasinca freq cap'i dusurur (acidan rampa).
    void setFreqCap(int cap_hz) { freq_cap_ = cap_hz; }
    void clearFreqCap()         { freq_cap_ = -1; }

private:
    enum State { ST_IDLE, ST_RAMP_UP, ST_CRUISE, ST_RAMP_DOWN, ST_REVERSING };

    State     state_       = ST_IDLE;
    int       current_freq_ = 0;
    int       target_freq_  = 0;
    int       freq_cap_     = -1;
    Direction current_dir_  = DIR_STOP;
    Direction pending_dir_  = DIR_STOP;
    int       pending_freq_ = 0;
    bool      servo_on_     = false;
    unsigned long ramp_start_ms_  = 0;
    int           ramp_start_freq_ = 0;
    unsigned long last_step_ms_   = 0;

    int  effectiveTarget() const;
    void applyHardware();          // LEDC + DIR + SON pinlerini yaz
    void writePulseFreq(int hz);   // 0 -> durdur, >0 -> kare dalga uret
};

#endif // MOTOR_DRIVER_H
