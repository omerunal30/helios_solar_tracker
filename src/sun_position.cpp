#include "sun_position.h"
#include <math.h>

namespace {
constexpr double DEG2RAD = 0.017453292519943295; // PI/180
constexpr double RAD2DEG = 57.29577951308232;    // 180/PI

// Verilen yerel tarih/saatten Julian Day (UTC bazli) hesaplar.
double computeJulianDay(const DateTimeUTC& dt) {
    // Yerel saat -> UTC saat
    double hour_utc = dt.hour - dt.utc_offset_hours
                      + (dt.minute + dt.second / 60.0) / 60.0;

    int Y = dt.year;
    int M = dt.month;
    int D = dt.day;

    // Gun kaymasini hesaba kat (UTC saat negatif veya >=24 olabilir)
    while (hour_utc < 0.0) {
        hour_utc += 24.0;
        D -= 1;
    }
    while (hour_utc >= 24.0) {
        hour_utc -= 24.0;
        D += 1;
    }

    if (M <= 2) {
        Y -= 1;
        M += 12;
    }

    double A = floor(Y / 100.0);
    double B = 2.0 - A + floor(A / 4.0);

    double JD = floor(365.25 * (Y + 4716))
              + floor(30.6001 * (M + 1))
              + D + B - 1524.5
              + hour_utc / 24.0;
    return JD;
}

double normalize360(double x) {
    x = fmod(x, 360.0);
    if (x < 0.0) x += 360.0;
    return x;
}

double normalize24(double x) {
    x = fmod(x, 24.0);
    if (x < 0.0) x += 24.0;
    return x;
}
} // namespace

SunAngles computeSunPosition(const DateTimeUTC& dt,
                              double latitude_deg,
                              double longitude_deg) {
    double JD = computeJulianDay(dt);
    double n = JD - 2451545.0; // J2000.0'dan beri gecen gun

    // Ortalama boylam (mean longitude)
    double L = normalize360(280.460 + 0.9856474 * n);
    // Ortalama anomali
    double g = normalize360(357.528 + 0.9856003 * n);
    // Ekliptik boylam
    double lambda = L + 1.915 * sin(g * DEG2RAD) + 0.020 * sin(2.0 * g * DEG2RAD);
    // Ekliptik egim
    double epsilon = 23.439 - 0.0000004 * n;

    // Sag acilim (right ascension) ve sapma (declination)
    double alpha = atan2(cos(epsilon * DEG2RAD) * sin(lambda * DEG2RAD),
                         cos(lambda * DEG2RAD)) * RAD2DEG;
    alpha = normalize360(alpha);
    double delta = asin(sin(epsilon * DEG2RAD) * sin(lambda * DEG2RAD)) * RAD2DEG;

    // Greenwich Mean Sidereal Time (saat)
    double GMST = normalize24(18.697374558 + 24.06570982441908 * n);
    // Yerel sidereal zaman (derece)
    double LMST_deg = normalize360(GMST * 15.0 + longitude_deg);
    // Saat acisi (hour angle)
    double H = LMST_deg - alpha;
    // -180..+180 araligina cek
    if (H > 180.0)  H -= 360.0;
    if (H < -180.0) H += 360.0;

    double lat_rad = latitude_deg * DEG2RAD;
    double dec_rad = delta * DEG2RAD;
    double H_rad   = H * DEG2RAD;

    // Yukseklik (elevation)
    double sin_alt = sin(lat_rad) * sin(dec_rad)
                   + cos(lat_rad) * cos(dec_rad) * cos(H_rad);
    if (sin_alt > 1.0)  sin_alt = 1.0;
    if (sin_alt < -1.0) sin_alt = -1.0;
    double altitude = asin(sin_alt);

    // Azimut (kuzeyden saat yonu)
    double cos_alt = cos(altitude);
    double azimuth = 0.0;
    if (cos_alt > 1e-9) {
        double cos_az = (sin(dec_rad) - sin_alt * sin(lat_rad)) / (cos_alt * cos(lat_rad));
        if (cos_az > 1.0)  cos_az = 1.0;
        if (cos_az < -1.0) cos_az = -1.0;
        azimuth = acos(cos_az) * RAD2DEG;
        // Sabah Dogu, ogleden sonra Bati duzeltmesi
        if (sin(H_rad) > 0.0) {
            azimuth = 360.0 - azimuth;
        }
    }

    // Tek eksen takip acisi (N-S yatay eksen, parabol oluk):
    // Gunesin Dogu-Yukari duzlemindeki izduusumunun acisi.
    // Konvansiyon: -90=Dogu, 0=Zenit, +90=Bati
    double azimuth_rad = azimuth * DEG2RAD;
    double s_east = cos(altitude) * sin(azimuth_rad);
    double s_up   = sin_alt;
    double track_rad = atan2(-s_east, s_up);

    SunAngles out;
    out.elevation_deg   = altitude * RAD2DEG;
    out.azimuth_deg     = azimuth;
    out.track_angle_deg = track_rad * RAD2DEG;
    return out;
}
