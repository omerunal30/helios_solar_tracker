#ifndef TRACKER_H
#define TRACKER_H

#include "motor_driver.h"
#include "position_sensor.h"
#include "rtc_clock.h"
#include "sun_position.h"
#include "temp_sensor.h"

// ============================================================
//  PARABOL OLUK TAKIP SISTEMI - ANA DURUM MAKINESI
//
//  Durumlar:
//   INIT          : ilk acilis, donanim kontrolu
//   HOME_FIND     : doguya git, HOME pozisyonunu bul
//   SUNRISE_WAIT  : gunes ufkun uzerine cikana kadar bekle
//   TRACKING      : gunesin takip acisini takip et
//   DEFOCUS       : asiri sicaklik -> gunesten sap, sicaklik dususune kadar bekle
//   STOW          : gunes battiktan sonra park pozisyonuna git, bekle
//   ERROR         : kritik hata
// ============================================================

class Tracker {
public:
    enum State {
        ST_INIT,
        ST_HOME_FIND,
        ST_SUNRISE_WAIT,
        ST_TRACKING,
        ST_DEFOCUS,
        ST_STOW,
        ST_ERROR
    };

    void begin(MotorDriver* motor,
               PositionSensor* pos,
               RtcClock* rtc,
               TempSensor* temp /* nullptr olabilir: sicaklik kontrolu devre disi */);

    void update();

    State state() const         { return state_; }
    const char* stateName() const;
    SunAngles lastSun() const   { return last_sun_; }
    double targetAngle() const  { return target_angle_deg_; }
    double currentAngle() const { return current_angle_deg_; }
    float  lastTemp() const     { return last_temp_c_; }
    bool   tempValid() const    { return temp_valid_; }

private:
    MotorDriver*    motor_ = nullptr;
    PositionSensor* pos_   = nullptr;
    RtcClock*       rtc_   = nullptr;
    TempSensor*     temp_  = nullptr;

    State state_ = ST_INIT;

    SunAngles last_sun_ {};
    double current_angle_deg_ = 0.0;
    double target_angle_deg_  = HOME_ANGLE;

    float  last_temp_c_   = 0.0f;
    bool   temp_valid_    = false;
    bool   overheated_    = false;     // hysteresis bayrak

    State  pre_defocus_state_ = ST_TRACKING;  // defocus'ten geri donulecek state

    unsigned long last_track_update_ms_ = 0;
    unsigned long last_temp_read_ms_    = 0;
    unsigned long state_entry_ms_       = 0;

    void enterState(State s);
    void driveTowardTarget();
    void driveTowardAngle(double tgt);
    bool refreshSunAndTarget();
    bool checkLimitsSafety();
    void updateTemperature();          // sicakligi oku ve overheated_ flag'i yonet
};

#endif // TRACKER_H
