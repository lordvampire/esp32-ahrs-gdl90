#include "bno086.h"
#include <math.h>

BNO086Handler::BNO086Handler()
    : _wire(nullptr),
      _roll(0.0f), _pitch(0.0f), _yaw(0.0f),
      _qi(0.0f), _qj(0.0f), _qk(0.0f), _qr(1.0f),
      _lastUpdateMs(0),
      _accuracy(0), _magAccuracy(0), _stability(0),
      _calibrating(false), _useGameRV(true) {}

void BNO086Handler::setUseGameRV(bool useGameRV) {
    _useGameRV = useGameRV;
}

bool BNO086Handler::begin(TwoWire &wire) {
    _wire = &wire;

    // BNO086 needs time to boot after power-on
    delay(1000);

    // Try up to 5 times — the BNO086 can be slow to fully initialize
    for (int attempt = 0; attempt < 5; attempt++) {
        if (_imu.begin(BNO086_ADDR, *_wire)) {
            delay(500);

            // Flight mode: only accelerometer dynamic calibration
            _imu.setCalibrationConfig(SH2_CAL_ACCEL);

            if (enableReport()) {
                // Stability classifier at 1 Hz
                _imu.enableStabilityClassifier(1000);

                // Consume the post-begin reset flag so the wasReset()
                // handler in update() doesn't fire before _sensor_value
                // is initialised by the first getSensorEvent() call.
                _imu.wasReset();

                _lastUpdateMs = millis();
                return true;
            }
        }
        delay(1000);
    }

    return false;
}

bool BNO086Handler::enableReport() {
    if (_useGameRV) {
        return _imu.enableARVRStabilizedGameRotationVector(BNO086_REPORT_INTERVAL_MS);
    } else {
        return _imu.enableARVRStabilizedRotationVector(BNO086_REPORT_INTERVAL_MS);
    }
}

bool BNO086Handler::reinit() {
    if (_wire == nullptr) return false;

    delay(1000);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (_imu.begin(BNO086_ADDR, *_wire)) {
            delay(500);

            // Restore calibration config
            if (_calibrating) {
                _imu.setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
            } else {
                _imu.setCalibrationConfig(SH2_CAL_ACCEL);
            }

            if (enableReport()) {
                _imu.enableStabilityClassifier(1000);
                if (_calibrating) _imu.enableMagnetometer(500);
                _imu.wasReset();  // consume reset flag
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
        // Re-configure everything after unexpected reset
        Serial.println("BNO086 reset detected, reinitializing...");
        if (_calibrating) {
            _imu.setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
        } else {
            _imu.setCalibrationConfig(SH2_CAL_ACCEL);
        }
        enableReport();
        _imu.enableStabilityClassifier(1000);
        if (_calibrating) _imu.enableMagnetometer(500);
    }

    if (!_imu.getSensorEvent()) return false;

    uint8_t id = _imu.getSensorEventID();

    // Rotation Vector (GRV or RV)
    if (id == SENSOR_REPORTID_AR_VR_STABILIZED_GAME_ROTATION_VECTOR ||
        id == SENSOR_REPORTID_AR_VR_STABILIZED_ROTATION_VECTOR) {
        _qi = _imu.getQuatI();
        _qj = _imu.getQuatJ();
        _qk = _imu.getQuatK();
        _qr = _imu.getQuatReal();
        _accuracy = _imu.getQuatAccuracy();
        quaternionToEuler(_qi, _qj, _qk, _qr);
        _lastUpdateMs = millis();
        return true;
    }

    // Magnetic Field — extract accuracy only
    if (id == SENSOR_REPORTID_MAGNETIC_FIELD) {
        _magAccuracy = _imu.getMagAccuracy();
        return false;  // no new attitude update
    }

    // Stability Classifier
    if (id == SENSOR_REPORTID_STABILITY_CLASSIFIER) {
        _stability = _imu.getStabilityClassifier();
        return false;
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
    // Roll (bank angle)
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

// --------------------------------------------------------------------------
// Calibration control
// --------------------------------------------------------------------------
void BNO086Handler::startCalibration() {
    _calibrating = true;
    _imu.setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
    _imu.enableMagnetometer(500);  // 2 Hz for live accuracy status
}

void BNO086Handler::stopCalibration() {
    _calibrating = false;
    _imu.setCalibrationConfig(SH2_CAL_ACCEL);  // flight mode
}

bool BNO086Handler::saveDCD() {
    return _imu.saveCalibration();
}

// --------------------------------------------------------------------------
// Getters
// --------------------------------------------------------------------------
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

void BNO086Handler::resetWatchdog() {
    _lastUpdateMs = millis();
}

uint8_t BNO086Handler::getAccuracy() const { return _accuracy; }
uint8_t BNO086Handler::getMagAccuracy() const { return _magAccuracy; }
uint8_t BNO086Handler::getStability() const { return _stability; }
bool BNO086Handler::isCalibrating() const { return _calibrating; }
