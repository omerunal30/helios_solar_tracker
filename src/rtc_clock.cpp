#include "rtc_clock.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

bool RtcClock::begin() {
    Wire.begin();
    // RTC adresine ping at
    Wire.beginTransmission(RTC_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

bool RtcClock::read(DateTimeUTC& out, int utc_offset_hours) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write((uint8_t)0x00); // saniye register'indan baslat
    if (Wire.endTransmission() != 0) return false;

    Wire.requestFrom((int)RTC_I2C_ADDR, 7);
    if (Wire.available() < 7) return false;

    uint8_t ss = Wire.read() & 0x7F;       // CH bit hariç
    uint8_t mm = Wire.read() & 0x7F;
    uint8_t hh_raw = Wire.read();
    uint8_t hh;
    if (hh_raw & 0x40) {
        // 12-saat modu
        uint8_t pm = (hh_raw & 0x20) ? 1 : 0;
        hh = bcd2dec(hh_raw & 0x1F);
        if (pm && hh < 12) hh += 12;
        if (!pm && hh == 12) hh = 0;
    } else {
        hh = bcd2dec(hh_raw & 0x3F);
    }
    Wire.read();                            // dow, kullanmiyoruz
    uint8_t dd = bcd2dec(Wire.read() & 0x3F);
    uint8_t mo_raw = Wire.read();
    uint8_t mo = bcd2dec(mo_raw & 0x1F);
    uint16_t yr = 2000 + bcd2dec(Wire.read());
    if (mo_raw & 0x80) yr += 100; // century bit

    out.year   = yr;
    out.month  = mo;
    out.day    = dd;
    out.hour   = hh;
    out.minute = bcd2dec(mm);
    out.second = bcd2dec(ss);
    out.utc_offset_hours = utc_offset_hours;
    return true;
}

bool RtcClock::write(const DateTimeUTC& dt) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write((uint8_t)0x00);
    Wire.write(dec2bcd(dt.second) & 0x7F);  // CH=0, oscillator calisir
    Wire.write(dec2bcd(dt.minute));
    Wire.write(dec2bcd(dt.hour) & 0x3F);    // 24h modu
    Wire.write((uint8_t)1);                 // dow (kullanmiyoruz)
    Wire.write(dec2bcd(dt.day));
    uint8_t mo = dec2bcd(dt.month);
    if (dt.year >= 2100) mo |= 0x80;
    Wire.write(mo);
    Wire.write(dec2bcd((uint8_t)(dt.year % 100)));
    return (Wire.endTransmission() == 0);
}

bool RtcClock::oscillatorStopped() {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write((uint8_t)0x0F);              // status register
    if (Wire.endTransmission() != 0) return true;
    Wire.requestFrom((int)RTC_I2C_ADDR, 1);
    if (Wire.available() < 1) return true;
    return (Wire.read() & 0x80) != 0;        // OSF bit
}
