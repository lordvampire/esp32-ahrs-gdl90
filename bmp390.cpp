#include "bmp390.h"
#include <math.h>

BMP390Handler::BMP390Handler()
    : _pressureHPa(1013.25f),
      _temperatureC(15.0f),
      _pressureAltFt(0.0f) {}

bool BMP390Handler::begin(TwoWire &wire) {
    if (!_bmp.begin_I2C(BMP390_ADDR, &wire)) {
        return false;
    }

    // Oversampling: pressure x8, temperature x1
    _bmp.setTemperatureOversampling(BMP3_NO_OVERSAMPLING);
    _bmp.setPressureOversampling(BMP3_OVERSAMPLING_8X);

    // IIR filter coefficient (smooth out turbulence noise)
    _bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);

    // Output data rate
    _bmp.setOutputDataRate(BMP3_ODR_50_HZ);

    return true;
}

bool BMP390Handler::update() {
    if (!_bmp.performReading()) {
        return false;
    }

    _pressureHPa = _bmp.pressure / 100.0f;  // Pa -> hPa
    _temperatureC = _bmp.temperature;
    _pressureAltFt = pressureToAltFt(_pressureHPa);
    return true;
}

// --------------------------------------------------------------------------
// ISA standard atmosphere pressure altitude.
// Formula: altitude_ft = (1 - (P / P0)^0.190284) * 145366.45
// where P0 = 1013.25 hPa (standard sea-level pressure).
// --------------------------------------------------------------------------
float BMP390Handler::pressureToAltFt(float pressureHPa) const {
    if (pressureHPa <= 0.0f) return 0.0f;
    return (1.0f - powf(pressureHPa / 1013.25f, 0.190284f)) * 145366.45f;
}

float BMP390Handler::getPressureHPa() const { return _pressureHPa; }
float BMP390Handler::getTemperatureC() const { return _temperatureC; }
float BMP390Handler::getPressureAltFt() const { return _pressureAltFt; }
