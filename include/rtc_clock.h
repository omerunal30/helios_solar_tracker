#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <stdint.h>
#include "sun_position.h"

// ============================================================
//  DS1302 RTC SURUCUSU (3-telli seri arayuz, kutuphane bagimliligi yok)
// ============================================================

class RtcClock {
public:
    // GPIO pinlerini ayarlar ve cihaz var mi diye RAM round-trip testi yapar.
    bool begin();

    // RTC'den anlik tarih/saati okur (clock burst). utc_offset config'den verilir.
    // Donus: true = okuma basarili, false = cihaz yok / mantiksiz deger
    bool read(DateTimeUTC& out, int utc_offset_hours);

    // RTC'ye tarih/saat yaz (manuel ayarlama icin).
    bool write(const DateTimeUTC& dt);

    // Clock Halt (CH) bit'ini kontrol et: true = oscillator durmus (pil bitmis/hic ayarlanmamis).
    bool oscillatorStopped();

private:
    static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }
};

#endif // RTC_CLOCK_H
