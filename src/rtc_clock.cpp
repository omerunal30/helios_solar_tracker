#include "rtc_clock.h"
#include "config.h"
#include <Arduino.h>

// ============================================================
//  DS1302 3-telli seri arayuz (CLK + DAT + RST/CE).
//  I2C/Wire YOK. Veri LSB-first gonderilir/okunur.
//
//  Komut baytı: [7]=1 [6]=RAM(1)/CK(0) [5:1]=adres [0]=RD(1)/WR(0)
//  Saat register'lari (yazma adresi cift, okuma = yazma|1):
//    0x80/0x81 saniye (bit7=CH)   0x82/0x83 dakika
//    0x84/0x85 saat              0x86/0x87 gun(tarih)
//    0x88/0x89 ay                0x8A/0x8B haftanin gunu
//    0x8C/0x8D yil               0x8E/0x8F WP (bit7=write protect)
//    0xBE/0xBF clock burst (8 bayt)
//    0xC0/0xC1 RAM[0] (var-yok testi icin)
// ============================================================

namespace {

constexpr int CLK = PIN_RTC_CLK;
constexpr int DAT = PIN_RTC_DAT;
constexpr int RST = PIN_RTC_RST;

inline void clockPulse() {
    digitalWrite(CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(CLK, LOW);
    delayMicroseconds(2);
}

// CE'yi yukari cek, transfer baslat (DAT cikis modunda).
void start() {
    digitalWrite(RST, LOW);
    digitalWrite(CLK, LOW);
    pinMode(DAT, OUTPUT);
    delayMicroseconds(4);
    digitalWrite(RST, HIGH);   // aktif HIGH; CE setup
    delayMicroseconds(4);
}

void stop() {
    digitalWrite(RST, LOW);
    delayMicroseconds(4);
}

// Bir bayti DAT'a yaz (LSB-first). DS1302 yukselen kenarda ornekler.
void shiftOut(uint8_t v) {
    pinMode(DAT, OUTPUT);
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(DAT, (v >> i) & 0x01);
        delayMicroseconds(2);
        clockPulse();
    }
}

// Bir bayti DAT'tan oku (LSB-first). Komut baytindan sonra ilk bit hazirdir.
uint8_t shiftIn() {
    uint8_t v = 0;
    pinMode(DAT, INPUT);
    for (uint8_t i = 0; i < 8; i++) {
        if (digitalRead(DAT)) v |= (1 << i);
        clockPulse();          // dusen kenarda DS1302 bir sonraki biti sunar
    }
    return v;
}

uint8_t readReg(uint8_t cmd_read) {
    start();
    shiftOut(cmd_read);
    uint8_t v = shiftIn();
    stop();
    return v;
}

void writeReg(uint8_t cmd_write, uint8_t value) {
    start();
    shiftOut(cmd_write);
    shiftOut(value);
    stop();
}

} // namespace

bool RtcClock::begin() {
    pinMode(CLK, OUTPUT);
    pinMode(RST, OUTPUT);
    digitalWrite(CLK, LOW);
    digitalWrite(RST, LOW);
    pinMode(DAT, OUTPUT);

    writeReg(0x8E, 0x00);  // write-protect kapat (yazma + RAM testi icin sart)

    // Var-yok testi: RAM[0]'a iki farkli desen yazip geri oku. Hat bos/sabit
    // takiliysa (0x00/0xFF) iki desenden en az biri tutmaz.
    uint8_t orig = readReg(0xC1);
    writeReg(0xC0, 0x5A);
    bool ok1 = (readReg(0xC1) == 0x5A);
    writeReg(0xC0, 0xA5);
    bool ok2 = (readReg(0xC1) == 0xA5);
    writeReg(0xC0, orig);  // kullanici RAM'ini geri yaz
    return ok1 && ok2;
}

bool RtcClock::read(DateTimeUTC& out, int utc_offset_hours) {
    start();
    shiftOut(0xBF);            // clock burst read
    uint8_t b[8];
    pinMode(DAT, INPUT);
    for (uint8_t i = 0; i < 8; i++) b[i] = shiftIn();
    stop();

    uint8_t ss = bcd2dec(b[0] & 0x7F);     // CH bit haric
    uint8_t mm = bcd2dec(b[1] & 0x7F);
    uint8_t hh;
    if (b[2] & 0x80) {                      // 12-saat modu
        uint8_t pm = (b[2] & 0x20) ? 1 : 0;
        hh = bcd2dec(b[2] & 0x1F);
        if (pm && hh < 12) hh += 12;
        if (!pm && hh == 12) hh = 0;
    } else {
        hh = bcd2dec(b[2] & 0x3F);          // 24-saat modu
    }
    uint8_t dd = bcd2dec(b[3] & 0x3F);
    uint8_t mo = bcd2dec(b[4] & 0x1F);
    uint16_t yr = 2000 + bcd2dec(b[6]);

    // Cihaz yok / hat bos ise burst 0xFF (veya 0x00) doner -> mantiksiz deger.
    if (ss > 59 || mm > 59 || hh > 23 ||
        dd < 1 || dd > 31 || mo < 1 || mo > 12) return false;

    out.year   = yr;
    out.month  = mo;
    out.day    = dd;
    out.hour   = hh;
    out.minute = mm;
    out.second = ss;
    out.utc_offset_hours = utc_offset_hours;
    return true;
}

bool RtcClock::write(const DateTimeUTC& dt) {
    writeReg(0x8E, 0x00);                              // WP kapat
    writeReg(0x80, dec2bcd(dt.second) & 0x7F);         // CH=0 -> oscillator calisir
    writeReg(0x82, dec2bcd(dt.minute) & 0x7F);
    writeReg(0x84, dec2bcd(dt.hour) & 0x3F);           // 24-saat modu (bit7=0)
    writeReg(0x86, dec2bcd(dt.day) & 0x3F);
    writeReg(0x88, dec2bcd(dt.month) & 0x1F);
    writeReg(0x8A, 0x01);                              // haftanin gunu (kullanmiyoruz)
    writeReg(0x8C, dec2bcd((uint8_t)(dt.year % 100)));

    // Geri oku ve dogrula (dakika ms icinde ilerlemez).
    return readReg(0x83) == (dec2bcd(dt.minute) & 0x7F);
}

bool RtcClock::oscillatorStopped() {
    return (readReg(0x81) & 0x80) != 0;     // CH (Clock Halt) bit
}
