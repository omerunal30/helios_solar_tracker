#include "tracker.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

void Tracker::begin(MotorDriver* motor,
                    PositionSensor* pos,
                    RtcClock* rtc,
                    TempSensor* temp) {
    motor_ = motor;
    pos_   = pos;
    rtc_   = rtc;
    temp_  = temp;
    enterState(ST_INIT);
}

const char* Tracker::stateName() const {
    switch (state_) {
        case ST_INIT:         return "INIT";
        case ST_HOME_FIND:    return "HOME_FIND";
        case ST_SUNRISE_WAIT: return "SUNRISE_WAIT";
        case ST_TRACKING:     return "TRACKING";
        case ST_DEFOCUS:      return "DEFOCUS";
        case ST_STOW:         return "STOW";
        case ST_ERROR:        return "ERROR";
    }
    return "?";
}

void Tracker::enterState(State s) {
    state_          = s;
    state_entry_ms_ = millis();
    if (motor_) motor_->stop();
}

bool Tracker::refreshSunAndTarget() {
    if (!rtc_) return false;
    DateTimeUTC dt;
    if (!rtc_->read(dt, UTC_OFFSET_HOURS)) return false;
    last_sun_ = computeSunPosition(dt, LATITUDE, LONGITUDE);
    double t = last_sun_.track_angle_deg;
    if (t < TRACK_ANGLE_MIN) t = TRACK_ANGLE_MIN;
    if (t > TRACK_ANGLE_MAX) t = TRACK_ANGLE_MAX;
    target_angle_deg_ = t;
    return true;
}

bool Tracker::checkLimitsSafety() {
    if (!motor_) return true;
    LimitState lim = readLimits();
    if (lim.east && target_angle_deg_ < current_angle_deg_) {
        motor_->stop();
        return false;
    }
    if (lim.west && target_angle_deg_ > current_angle_deg_) {
        motor_->stop();
        return false;
    }
    return true;
}

void Tracker::driveTowardAngle(double tgt) {
    if (!motor_ || !pos_) return;
    current_angle_deg_ = pos_->readAngle();
    double err = tgt - current_angle_deg_;
    double abs_err = fabs(err);

    if (abs_err <= ANGLE_TOLERANCE) {
        motor_->stop();
        return;
    }

    // Hedefe yaklasildikca pulse frekansini (cap) lineer dusur.
    int freq_cap;
    if (abs_err >= ANGLE_RAMP_ZONE) {
        freq_cap = PULSE_FREQ_MAX;
    } else {
        double t = abs_err / ANGLE_RAMP_ZONE;
        freq_cap = (int)(PULSE_FREQ_MIN + t * (PULSE_FREQ_MAX - PULSE_FREQ_MIN));
    }
    motor_->setFreqCap(freq_cap);

    MotorDriver::Direction dir = (err > 0)
        ? MotorDriver::DIR_WEST
        : MotorDriver::DIR_EAST;
    motor_->setTarget(PULSE_FREQ_MAX, dir);
}

void Tracker::driveTowardTarget() {
    driveTowardAngle(target_angle_deg_);
}

void Tracker::updateTemperature() {
    if (!temp_) {
        temp_valid_ = false;
        overheated_ = false;
        return;
    }
    unsigned long now = millis();
    if (now - last_temp_read_ms_ < TEMP_READ_INTERVAL_MS) return;
    last_temp_read_ms_ = now;

    float c;
    bool ok = temp_->read(c);
    if (!ok) {
        temp_valid_ = false;
        // Sensor okunamiyorsa konservatif: defocus tetikleme
        return;
    }
    last_temp_c_ = c;
    temp_valid_  = true;

    // Sensor hatasi (mantiksiz okuma) -> ariza
    if (c > TEMP_ERROR_MAX || c < -50.0f) {
        temp_valid_ = false;
        return;
    }

    // Histerezis: HIGH'i gecince overheated, LOW'un altina dusunce normal
    if (!overheated_ && c >= TEMP_DEFOCUS_HIGH) overheated_ = true;
    if ( overheated_ && c <= TEMP_RESUME_LOW)   overheated_ = false;
}

void Tracker::update() {
    if (!motor_ || !pos_ || !rtc_) return;

    motor_->update();
    current_angle_deg_ = pos_->readAngle();
    updateTemperature();
    checkLimitsSafety();

    // GLOBAL: TRACKING durumundayken sicaklik yuksekse defocus'a gec
    if (state_ == ST_TRACKING && overheated_) {
        pre_defocus_state_ = ST_TRACKING;
        enterState(ST_DEFOCUS);
    }

    switch (state_) {

    case ST_INIT: {
        DateTimeUTC dt;
        if (!rtc_->read(dt, UTC_OFFSET_HOURS) || rtc_->oscillatorStopped()) {
            enterState(ST_ERROR);
            return;
        }
        // Motor SON sinyali aktif et
        motor_->setServoOn(true);
        enterState(ST_HOME_FIND);
        break;
    }

    case ST_HOME_FIND: {
        LimitState lim = readLimits();
        if (lim.east || current_angle_deg_ <= HOME_ANGLE + ANGLE_TOLERANCE) {
            motor_->stop();
            if (!motor_->isMoving()) {
                enterState(ST_SUNRISE_WAIT);
            }
        } else {
            driveTowardAngle(HOME_ANGLE);
        }
        break;
    }

    case ST_SUNRISE_WAIT: {
        unsigned long now = millis();
        if (now - last_track_update_ms_ >= TRACK_INTERVAL_MS) {
            last_track_update_ms_ = now;
            if (!refreshSunAndTarget()) {
                enterState(ST_ERROR);
                return;
            }
            if (last_sun_.elevation_deg >= SUNRISE_MIN_ELEVATION) {
                enterState(ST_TRACKING);
            }
        }
        motor_->stop();
        break;
    }

    case ST_TRACKING: {
        unsigned long now = millis();
        if (now - last_track_update_ms_ >= TRACK_INTERVAL_MS) {
            last_track_update_ms_ = now;
            if (!refreshSunAndTarget()) {
                motor_->stop();
                break;
            }
            if (last_sun_.elevation_deg < SUNSET_MIN_ELEVATION) {
                enterState(ST_STOW);
                return;
            }
        }
        driveTowardTarget();
        break;
    }

    case ST_DEFOCUS: {
        // Sicaklik dustugunde TRACKING'e geri don.
        if (!overheated_) {
            enterState(pre_defocus_state_);
            return;
        }
        // Defocus = DEFOCUS_PARK_ANGLE (zenit, 0°). Parabol acikligi yukari
        // bakar, gunes hangi pozisyonda olursa olsun odak boruya yansima
        // toplanmaz. Sabit hedef oldugu icin gunes zenitten gecerken olusan
        // sicrama/salinim problemi yok. STOW'dan farkli; STOW = ertesi
        // gunun baslangic pozisyonu, DEFOCUS = anlik termal guvenlik.
        target_angle_deg_ = DEFOCUS_PARK_ANGLE;
        driveTowardAngle(DEFOCUS_PARK_ANGLE);

        // Gunes battiysa burda da stow'a gec
        unsigned long now = millis();
        if (now - last_track_update_ms_ >= TRACK_INTERVAL_MS) {
            last_track_update_ms_ = now;
            if (refreshSunAndTarget() &&
                last_sun_.elevation_deg < SUNSET_MIN_ELEVATION) {
                enterState(ST_STOW);
            }
        }
        break;
    }

    case ST_STOW: {
        target_angle_deg_ = STOW_ANGLE;
        driveTowardAngle(STOW_ANGLE);

        if (fabs(current_angle_deg_ - STOW_ANGLE) <= ANGLE_TOLERANCE && !motor_->isMoving()) {
            unsigned long now = millis();
            // Stow'da iken dakikalik bir periyotla gunesi kontrol et
            if (now - last_track_update_ms_ >= (TRACK_INTERVAL_MS * 6)) {
                last_track_update_ms_ = now;
                if (refreshSunAndTarget() &&
                    last_sun_.elevation_deg >= SUNRISE_MIN_ELEVATION) {
                    // Yeni gun -> doguya don ve takibe basla
                    enterState(ST_HOME_FIND);
                }
            }
        }
        break;
    }

    case ST_ERROR: {
        motor_->emergencyStop();
        unsigned long now = millis();
        if (now - state_entry_ms_ >= 5000) {
            state_entry_ms_ = now;
            DateTimeUTC dt;
            if (rtc_->read(dt, UTC_OFFSET_HOURS) && !rtc_->oscillatorStopped()) {
                motor_->setServoOn(true);
                enterState(ST_HOME_FIND);
            }
        }
        break;
    }
    }
}
