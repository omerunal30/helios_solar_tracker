#ifndef SUN_POSITION_H
#define SUN_POSITION_H

// ============================================================
//  GUNES POZISYON HESAPLAMA
//  NOAA tabanli, RTC zamani + enlem/boylam'dan
//  - solar elevation (yukseklik)
//  - solar azimuth (yatay aci, kuzeyden saat yonu)
//  - tek eksen takip acisi (N-S yatay eksen, parabol oluk icin)
// ============================================================

struct SunAngles {
    double elevation_deg;   // ufuk uzerinde derece (negatif = ufuk altinda)
    double azimuth_deg;     // kuzeyden saat yonunde derece (0=N, 90=E, 180=S, 270=W)
    double track_angle_deg; // tek eksen takip acisi: -90=Dogu, 0=Zenit, +90=Bati
};

struct DateTimeUTC {
    int year;     // 4 haneli
    int month;    // 1-12
    int day;      // 1-31
    int hour;     // 0-23 (yerel saat)
    int minute;   // 0-59
    int second;   // 0-59
    int utc_offset_hours;
};

// Verilen tarih/saat ve konumdan gunes pozisyonunu hesaplar.
// latitude_deg: kuzey pozitif, longitude_deg: dogu pozitif.
SunAngles computeSunPosition(const DateTimeUTC& dt,
                              double latitude_deg,
                              double longitude_deg);

#endif // SUN_POSITION_H
