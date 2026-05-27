# Helios

Tek eksenli parabol oluk güneş takip sistemi. ESP32 + Delta ASDA-B2 servo + DS3231 RTC + termokupl tabanlı odak sıcaklığı korumalı.

Sistem RTC'den aldığı tarih/saat ile NOAA güneş pozisyon algoritmasını çalıştırır, parabol oluğun N-S yatay dönme ekseni etrafında güneşi Doğu→Batı izleyeceği açıyı hesaplar ve trapez hız profili ile servo motoru yumuşakça konumlandırır. Odak borusu aşırı ısındığında otomatik defocus yapar, soğuyunca takibi sürdürür. Akşam güneş battığında ertesi sabahın başlangıç konumuna (Doğu home) geri sarar.

## Donanım

| Bileşen | Model | Bağlantı |
|---|---|---|
| MCU | ESP32 DevKit | — |
| Servo sürücü | Delta ASDA-B2 | Pulse + Direction (Pt mode) |
| RTC | DS3231 | I2C (SDA=21, SCL=22) |
| Pozisyon | Potansiyometre veya quadrature encoder | ADC1 / GPIO interrupt |
| Sıcaklık | MAX6675 K-tipi termokupl | SPI |
| Limit | 2× endüktif/mekanik switch | Doğu + Batı uç |

ESP32 GPIO 3.3V, ASDA-B2 girişleri 5V/24V optoizoleli — aralarına seviye uydurucu (74HC125, 4N25 opto vs.) **şarttır**, aksi halde drive tetiklenmez.

### Pin haritası (varsayılan, `include/config.h`)

| Sinyal | GPIO | Yön | Notu |
|---|---|---|---|
| PULSE → ASDA PULSE+ | 25 | OUT | LEDC kare dalga |
| DIR → ASDA SIGN+ | 26 | OUT | HIGH=Batı, LOW=Doğu |
| SON → ASDA Servo On | 27 | OUT | Optoizoleli |
| Limit Doğu | 32 | IN PULLUP | Basılı = LOW |
| Limit Batı | 33 | IN PULLUP | Basılı = LOW |
| Potansiyometre | 34 | ADC | ADC1, 12-bit |
| Encoder A / B | 35 / 39 | IN | İstege bağlı, `USE_ENCODER` define edilirse |
| MAX6675 CS | 5 | OUT | SPI CS |
| MAX6675 SCK | 18 | SCK | VSPI default |
| MAX6675 MISO | 19 | MISO | VSPI default |
| I2C SDA / SCL | 21 / 22 | — | DS3231 |

## Optik / Mekanik

6 m genişlik × 3 sıra × 6 m = 18 m uzunluk parabol oluk. **18 m boyu Kuzey-Güney yönlü** döşenir, oluk bu eksen etrafında döner ve aperture'sini Doğu→Batı sürer. Odak borusu (kalın metal, taşıyıcı sıvı için) parabol odak hattı boyunca uzanır.

Açı konvansiyonu:

```
  ZENIT (0°)
       |
   ___ | ___
  /    |    \
 /     |     \
DOĞU   |    BATI
(-90°) | (+90°)
```

## Kurulum

```bash
# PlatformIO ile (önerilir)
pio run                 # derle
pio run -t upload       # ESP32'ye yükle
pio device monitor      # seri monitör (115200 baud)
```

İlk açılışta RTC'nin saatini ayarlamak için `src/main.cpp` içindeki yorumlu bloğu bir kez aç:

```cpp
DateTimeUTC dt{2026, 5, 27, 12, 0, 0, UTC_OFFSET_HOURS};
rtc.write(dt);
```

Bir defa flash'le, sonra yorumla geri kapat. RTC pil yedekli olduğu için bir daha gerek olmaz.

## Konfigürasyon

Saha bilgileri ve kalibrasyon değerleri tek dosyada: [`include/config.h`](include/config.h).

```cpp
LATITUDE  = 39.9334;   // Enlem
LONGITUDE = 32.8597;   // Boylam
UTC_OFFSET_HOURS = 3;

GEAR_RATIO = 100.0;    // Motor:eksen redüktör oranı (saha ölçümü gerekir!)
PULSE_FREQ_MAX = 3000; // ~1.08 deg/s slew hızı

TEMP_DEFOCUS_HIGH = 350.0f;  // Bu sıcaklığın üstünde defocus
TEMP_RESUME_LOW   = 280.0f;  // Bu sıcaklığın altında tekrar takip (histerezis)
```

### Sahada kalibrasyon

1. **`GEAR_RATIO`**: Motoru elle 1 tam tur çevir (10000 pulse), oluğun kaç derece döndüğünü ölç. `360 / oluk_dönmesi_derece` = gear ratio. Örnek: 10000 pulse'ta oluk 3.6° dönerse, ratio = 100.
2. **`POT_ANGLE_MIN/MAX`**: Oluğu doğu limite götür, ADC raw değerini oku → `POT_ANGLE_MIN = -90`'a karşılık gelir. Batı limit için aynısı.
3. **Yön polaritesi**: İlk kalkışta motor ters yöne giderse `motor_driver.cpp` içindeki `digitalWrite(PIN_DIR, ...)` polaritesini ters çevir veya ASDA P1-01 yön bit'ini değiştir.

### ASDA-B2 sürücü parametreleri

Drive panelinden bir kez ayarlanır:

| Parametre | Değer | Açıklama |
|---|---|---|
| P1-00 | 0x0002 | PULSE + SIGN format |
| P1-01 | 0x0000 | Pt mode (external position) |
| P1-44 | 1 | Elektronik dişli sayaç |
| P1-45 | 1 | Elektronik dişli payda |
| P1-32 | 0x0000 | Stop mode |
| P2-15..17 | DI atamaları | CWL, CCWL, EMGS için |

## Sistem mimarisi

```
include/
  config.h          Tüm konfigürasyon (pinler, eşikler, kalibrasyon)
  sun_position.h    NOAA güneş pozisyon algoritması API'si
  rtc_clock.h       DS3231 sürücü API'si
  motor_driver.h    Pulse+dir motor + trapez rampa API'si
  position_sensor.h Potansiyometre/encoder + limit switch API'si
  temp_sensor.h     Soyut TempSensor + MAX6675 + Stub
  tracker.h         Ana durum makinesi
src/
  sun_position.cpp  Julian Day → declination → hour angle → elev/az → track açısı
  rtc_clock.cpp     I2C BCD okuma/yazma
  motor_driver.cpp  ESP32 LEDC ile değişken frekans pulse + ramp state machine
  position_sensor.cpp ADC okuma + ISR'li encoder
  temp_sensor.cpp   MAX6675 SPI okuma (kütüphane bağımlılığı yok)
  tracker.cpp       INIT→HOME_FIND→SUNRISE_WAIT→TRACKING→[DEFOCUS]→STOW
  main.cpp          Arduino setup/loop + seri telemetri
  test_host.cpp     Host PC'de güneş algoritması doğrulama (Arduino gerek yok)
simulation.html     Tarayıcıda interaktif görsel simulasyon
platformio.ini      ESP32 build config
```

## Durum makinesi

```
       +------+
       | INIT |
       +--+---+
          |  RTC OK
          v
   +-------------+         +--------------+
   | HOME_FIND   |-------->| SUNRISE_WAIT |
   +-------------+ home'da +-------+------+
                                   |  elev >= 2°
                                   v
                          +-----------------+      +---------+
                          |    TRACKING     |<---->| DEFOCUS |
                          +--------+--------+      +---------+
                                   |  elev < 2°  ^   sıcaklık histerezisi
                                   v             |
                          +-----------------+    |
                          |      STOW       |----+ (yeni gün)
                          | (HOME konumu)   |
                          +-----------------+
```

Çift seviyeli rampa:
1. **Motor seviyesi**: Pulse frekansı `PULSE_FREQ_MIN`'den `PULSE_FREQ_MAX`'a trapez profil (her start/stop'ta).
2. **Açı seviyesi**: Hedefe `ANGLE_RAMP_ZONE` (3°) kala motor freq cap'i lineer düşer → oluk hedefe pürüzsüz oturur.

## Simulasyon

`simulation.html` dosyasını tarayıcıda aç (`open simulation.html` veya çift tıkla). C++ kodunun bire bir JavaScript portu:

- Yan görünüm: parabol kesidi + odak borusu (sıcaklıkla rengi değişir) + güneş
- Telemetri paneli + sim hızı (1× → 7200×)
- Gün boyu grafik: güneş elev, hedef açı, anlık açı, sıcaklık
- Manuel sıcaklık slider'ı → DEFOCUS state'ini test et

Embedded kodu açıp Arduino'ya yüklemeden algoritmayı görsel olarak test edebilirsin.

## Host PC üzerinde algoritma testi

```bash
g++ -std=c++14 -O2 -Iinclude src/sun_position.cpp src/test_host.cpp -o test_host
./test_host
```

Ankara için ekinoks/gündönümü günleri saatlik elev/azimut/track açıları yazar. Beklenen değerlerle (öğle elev = 90 - lat ± deklinasyon) doğrular.

## Telemetri (seri çıktı)

```
state=TRACKING elev=42.18 tgt=-23.45 cur=-23.41 freq=820 temp=187.3
```

| Alan | Anlam |
|---|---|
| `state` | Durum makinesi pozisyonu |
| `elev` | Güneş yüksekliği (°) |
| `tgt` | Hedef oluk açısı (°) |
| `cur` | Anlık oluk açısı (°) |
| `freq` | Pulse frekansı (Hz, motor hızı) |
| `temp` | Odak borusu sıcaklığı (°C), `--` = sensör hatası |

## Güvenlik notları

- Limit switch'leri ASDA'nın CWL/CCWL girişlerine de paralel bağla — yazılım koruması başarısız olursa servo kendi kendini durdurur.
- `EMGS` (acil stop) girişine NC mantar buton bağla.
- Odak borusunun sıcaklığı boru malzemesi sınırını geçmemeli — `TEMP_DEFOCUS_HIGH`'i borunun max çalışma sıcaklığının altında ayarla.
- ESP32 ↔ ASDA arasında **mutlaka** seviye uydurucu kullan, doğrudan bağlantı çalışmaz.
