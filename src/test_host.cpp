// ============================================================
//  HOST PC TEST PROGRAMI
//  sun_position algoritmasini RTC/Arduino donanimi olmadan dogrular.
//
//  Derleme:
//    g++ -std=c++14 -O2 -Iinclude src/sun_position.cpp src/test_host.cpp -o test_host
//  Calistir:
//    ./test_host
// ============================================================

#include "sun_position.h"
#include <cstdio>
#include <cmath>

static void printDay(int year, int month, int day, double lat, double lon, int utc_off) {
    printf("\n=== %04d-%02d-%02d  lat=%.4f lon=%.4f UTC%+d ===\n",
           year, month, day, lat, lon, utc_off);
    printf("%-6s %-9s %-9s %-9s\n", "saat", "elev", "azimut", "track");
    for (int h = 4; h <= 21; ++h) {
        DateTimeUTC dt{year, month, day, h, 0, 0, utc_off};
        SunAngles s = computeSunPosition(dt, lat, lon);
        printf("%02d:00  %+8.3f  %8.3f  %+8.3f\n",
               h, s.elevation_deg, s.azimuth_deg, s.track_angle_deg);
    }
}

static void noonCheck(int year, int month, int day, double lat, double lon, int utc_off,
                     double expect_elev, double expect_az_near_south) {
    DateTimeUTC dt{year, month, day, 12, 0, 0, utc_off};
    SunAngles s = computeSunPosition(dt, lat, lon);
    printf("[noon-check %04d-%02d-%02d] elev=%.2f az=%.2f track=%.2f"
           " (beklenen elev~%.1f, az~%.0f)\n",
           year, month, day, s.elevation_deg, s.azimuth_deg, s.track_angle_deg,
           expect_elev, expect_az_near_south);
}

int main() {
    // Ankara: 39.9334 N, 32.8597 E, UTC+3
    const double lat = 39.9334;
    const double lon = 32.8597;
    const int    utc = 3;

    // Gun boyu profil: ilkbahar ekinoks
    printDay(2026, 3, 20, lat, lon, utc);
    // Gun boyu profil: yaz gunestoresi
    printDay(2026, 6, 21, lat, lon, utc);
    // Gun boyu profil: kis gunestoresi
    printDay(2026, 12, 21, lat, lon, utc);

    printf("\n--- Akil saglamasi (Ankara yerel ogle ~12:00 + EoT) ---\n");
    // Ekinokslarda ogle elev = 90 - lat = 50.07
    noonCheck(2026, 3, 20, lat, lon, utc, 50.07, 180.0);
    // Yaz gunestores: 90 - lat + 23.44 = 73.51
    noonCheck(2026, 6, 21, lat, lon, utc, 73.51, 180.0);
    // Kis gunestores: 90 - lat - 23.44 = 26.63
    noonCheck(2026, 12, 21, lat, lon, utc, 26.63, 180.0);

    printf("\n--- Ekvator testi (lat=0, ekinokstaki ogle: elev~90) ---\n");
    noonCheck(2026, 3, 20, 0.0, 0.0, 0, 90.0, 180.0);

    return 0;
}
