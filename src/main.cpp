#include <Arduino.h>
#include "config.h"
#include "rtc_clock.h"
#include "motor_driver.h"
#include "position_sensor.h"
#include "temp_sensor.h"
#include "tracker.h"

// Donanim instance'lari
static RtcClock          rtc;
static MotorDriver       motor;
static PositionSensor    pos;
static TempSensorMax6675 temp_sensor;     // sicaklik sensoru karari gelince burayi degistir
static Tracker           tracker;

static unsigned long last_log_ms = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== Parabol Oluk Tracker (ESP32 + ASDA-B2) ==="));

    if (!rtc.begin()) {
        Serial.println(F("HATA: DS3231 RTC bulunamadi (I2C SDA=21 SCL=22 kontrol et)"));
    } else if (rtc.oscillatorStopped()) {
        Serial.println(F("UYARI: RTC oscillator durmus, saati ayarlamak gerek."));
        // Ilk kurulumda manuel ayarlamak icin:
        // DateTimeUTC dt{2026, 5, 27, 12, 0, 0, UTC_OFFSET_HOURS};
        // rtc.write(dt);
    } else {
        Serial.println(F("RTC OK"));
    }

    motor.begin();
    pos.begin();
    temp_sensor.begin();
    tracker.begin(&motor, &pos, &rtc, &temp_sensor);

    Serial.println(F("Init tamam. Loop basliyor."));
}

void loop() {
    tracker.update();

    unsigned long now = millis();
    if (now - last_log_ms >= 1000) {
        last_log_ms = now;
        SunAngles s = tracker.lastSun();
        Serial.print(F("state="));
        Serial.print(tracker.stateName());
        Serial.print(F(" elev="));
        Serial.print(s.elevation_deg, 2);
        Serial.print(F(" tgt="));
        Serial.print(tracker.targetAngle(), 2);
        Serial.print(F(" cur="));
        Serial.print(tracker.currentAngle(), 2);
        Serial.print(F(" freq="));
        Serial.print(motor.currentFreq());
        Serial.print(F(" temp="));
        if (tracker.tempValid()) Serial.print(tracker.lastTemp(), 1);
        else                     Serial.print(F("--"));
        Serial.println();
    }
}
