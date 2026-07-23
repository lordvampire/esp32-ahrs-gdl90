#ifndef BMP390_H
#define BMP390_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP3XX.h>

// I2C address for BMP390 (0x76 = SDO low/GND, 0x77 = SDO high/VCC)
#define BMP390_ADDR 0x76

class BMP390Handler {
public:
    BMP390Handler();

    // Initialize the sensor. Returns true on success.
    bool begin(TwoWire &wire = Wire);

    // Read new pressure/temperature. Returns true if successful.
    bool update();

    // Pressure in hPa (millibars)
    float getPressureHPa() const;

    // Temperature in degrees Celsius
    float getTemperatureC() const;

    // Pressure altitude in feet (ISA standard atmosphere, QNH 1013.25 hPa)
    float getPressureAltFt() const;

private:
    Adafruit_BMP3XX _bmp;
    float _pressureHPa;
    float _temperatureC;
    float _pressureAltFt;

    // ISA conversion: pressure (hPa) -> altitude (feet)
    float pressureToAltFt(float pressureHPa) const;
};

#endif
