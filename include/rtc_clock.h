#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <stdint.h>
#include "sun_position.h"

// ============================================================
//  DS3231 RTC SURUCUSU (I2C uzerinden, Wire kutuphanesi)
// ============================================================

class RtcClock {
public:
    // I2C baslat (varsayilan Wire). begin() Wire.begin() cagirir.
    bool begin();

    // RTC'den anlik tarih/saati okur. utc_offset config'den verilir.
    // Donus: true = okuma basarili, false = I2C hatasi
    bool read(DateTimeUTC& out, int utc_offset_hours);

    // RTC'ye tarih/saat yaz (manuel ayarlama icin).
    bool write(const DateTimeUTC& dt);

    // RTC'nin oscillator stop flag'ini kontrol et (pil bitmis mi?)
    bool oscillatorStopped();

private:
    static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }
};

#endif // RTC_CLOCK_H
