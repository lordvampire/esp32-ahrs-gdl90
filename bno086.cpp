#include "bno086.h"
#include <math.h>

BNO086Handler::BNO086Handler()
    : _wire(nullptr),
      _roll(0.0f), _pitch(0.0f), _yaw(0.0f),
      _qi(0.0f), _qj(0.0f), _qk(0.0f), _qr(1.0f),
      _lastUpdateMs(0) {}

bool BNO086Handler::begin(TwoWire &wire) {
    _wire = &wire;

    // BNO086 needs time to boot after power-on
    delay(1000);

    // Try up to 5 times — the BNO086 can be slow to fully initialize
    for (int attempt = 0; attempt < 5; attempt++) {
        if (_imu.begin(BNO086_ADDR, *_wire)) {
            delay(500);
            if (enableReport()) {
                _lastUpdateMs = millis();
                return true;
            }
        }
        delay(1000);
    }

    return false;
}

bool BNO086Handler::enableReport() {
    // ARVR Stabilized Rotation Vector: best stability for aircraft AHRS.
    // Uses accelerometer + gyroscope + magnetometer, game-rotation stabilized.
    if (!_imu.enableARVRStabilizedRotationVector(BNO086_REPORT_INTERVAL_MS)) {
        return false;
    }
    return true;
}

bool BNO086Handler::reinit() {
    if (_wire == nullptr) return false;

    delay(1000);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (_imu.begin(BNO086_ADDR, *_wire)) {
            delay(500);
            if (enableReport()) {
                _lastUpdateMs = millis();
                return true;
            }
        }
        delay(1000);
    }
    return false;
}

bool BNO086Handler::update() {
    if (_imu.wasReset()) {
        // After a reset, re-enable our reports.
        enableReport();
    }

    if (!_imu.getSensorEvent()) {
        return false;
    }

    if (_imu.getSensorEventID() == SENSOR_REPORTID_AR_VR_STABILIZED_ROTATION_VECTOR) {
        _qi = _imu.getQuatI();
        _qj = _imu.getQuatJ();
        _qk = _imu.getQuatK();
        _qr = _imu.getQuatReal();

        quaternionToEuler(_qi, _qj, _qk, _qr);
        _lastUpdateMs = millis();
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------
// Quaternion (i, j, k, real) -> Euler angles (roll, pitch, yaw) in degrees.
// Uses aerospace convention:
//   Roll  = rotation about X (forward), positive = right wing down
//   Pitch = rotation about Y (right),   positive = nose up
//   Yaw   = rotation about Z (down),    0-360 degrees
// --------------------------------------------------------------------------
void BNO086Handler::quaternionToEuler(float qi, float qj, float qk, float qr) {
    // Roll (bank angle) — raw value from quaternion.
    // Sign inversion for aircraft convention (mounting-dependent) is
    // handled by config.invertRoll in the main sketch.
    float sinr_cosp = 2.0f * (qr * qi + qj * qk);
    float cosr_cosp = 1.0f - 2.0f * (qi * qi + qj * qj);
    _roll = atan2f(sinr_cosp, cosr_cosp) * (180.0f / M_PI);

    // Pitch
    float sinp = 2.0f * (qr * qj - qk * qi);
    if (fabsf(sinp) >= 1.0f) {
        _pitch = copysignf(90.0f, sinp);
    } else {
        _pitch = asinf(sinp) * (180.0f / M_PI);
    }

    // Yaw (heading)
    float siny_cosp = 2.0f * (qr * qk + qi * qj);
    float cosy_cosp = 1.0f - 2.0f * (qj * qj + qk * qk);
    _yaw = atan2f(siny_cosp, cosy_cosp) * (180.0f / M_PI);

    // Normalize yaw to 0-360
    if (_yaw < 0.0f) _yaw += 360.0f;
}

float BNO086Handler::getRoll() const { return _roll; }
float BNO086Handler::getPitch() const { return _pitch; }
float BNO086Handler::getYaw() const { return _yaw; }

float BNO086Handler::getQI() const { return _qi; }
float BNO086Handler::getQJ() const { return _qj; }
float BNO086Handler::getQK() const { return _qk; }
float BNO086Handler::getQReal() const { return _qr; }

unsigned long BNO086Handler::getLastUpdateMs() const { return _lastUpdateMs; }

bool BNO086Handler::isWatchdogExpired() const {
    return (millis() - _lastUpdateMs) > BNO086_WATCHDOG_MS;
}
