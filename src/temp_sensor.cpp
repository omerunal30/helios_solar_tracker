#include "temp_sensor.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

// ============================================================
//  MAX6675 SPI driver (kutuphane bagimliligi yok, direkt SPI)
//
//  MAX6675 16-bit veri formati (MSB first):
//    [15]   dummy (=0)
//    [14:3] 12-bit sicaklik (0.25 C cozunurluk, 0..4095)
//    [2]    1 = termokupl bagli degil (open circuit)
//    [1]    device ID (0)
//    [0]    state
// ============================================================

void TempSensorMax6675::begin() {
    pinMode(PIN_TEMP_CS, OUTPUT);
    digitalWrite(PIN_TEMP_CS, HIGH);
    SPI.begin(PIN_TEMP_SCK, PIN_TEMP_MISO, /*MOSI*/ -1, PIN_TEMP_CS);
    // MAX6675 read-only, MOSI'ye ihtiyaci yok
}

bool TempSensorMax6675::read(float& out_celsius) {
    // MAX6675 maksimum ~4.3 MHz SPI clock kabul eder. 1 MHz guvenli.
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_TEMP_CS, LOW);
    delayMicroseconds(2);
    uint8_t hi = SPI.transfer(0x00);
    uint8_t lo = SPI.transfer(0x00);
    digitalWrite(PIN_TEMP_CS, HIGH);
    SPI.endTransaction();

    uint16_t raw = ((uint16_t)hi << 8) | lo;

    // Bit 2 set: termokupl baglanti hatasi
    if (raw & 0x0004) {
        out_celsius = 0.0f;
        return false;
    }

    // Bit 15..3 -> sicaklik
    uint16_t temp_raw = (raw >> 3) & 0x0FFF;
    out_celsius = temp_raw * 0.25f;
    return true;
}
