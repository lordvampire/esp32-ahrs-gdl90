#ifndef BNO086_H
#define BNO086_H

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

// I2C address for BNO086 on Qwiic chain
#define BNO086_ADDR 0x4B

// Report interval in milliseconds (20ms = 50Hz)
#define BNO086_REPORT_INTERVAL_MS 20

// Watchdog timeout: re-init if no data for this many milliseconds
#define BNO086_WATCHDOG_MS 2000

class BNO086Handler {
public:
    BNO086Handler();

    // Set sensor fusion mode before calling begin()
    void setUseGameRV(bool useGameRV);

    // Initialize the sensor. Returns true on success.
    bool begin(TwoWire &wire = Wire);

    // Call in loop. Returns true if new attitude data was read.
    bool update();

    // Attempt to re-initialize (for watchdog recovery).
    bool reinit();

    // Attitude in degrees
    float getRoll() const;
    float getPitch() const;
    float getYaw() const;

    // Raw quaternion components
    float getQI() const;
    float getQJ() const;
    float getQK() const;
    float getQReal() const;

    // Timestamp of last successful read (millis)
    unsigned long getLastUpdateMs() const;

    // Check if watchdog expired (no data for BNO086_WATCHDOG_MS)
    bool isWatchdogExpired() const;

    // Accuracy (0-3) for quaternion and magnetometer
    uint8_t getAccuracy() const;
    uint8_t getMagAccuracy() const;

    // Stability classifier (0=unknown, 1=table, 2=stationary, 3=stable, 4=motion)
    uint8_t getStability() const;

    // Calibration mode control
    void startCalibration();    // enable accel+gyro+mag dynamic cal
    void stopCalibration();     // set flight mode: accel-only cal
    bool saveDCD();             // save DCD to flash
    bool isCalibrating() const;

private:
    BNO08x _imu;
    TwoWire *_wire;

    float _roll;
    float _pitch;
    float _yaw;
    float _qi, _qj, _qk, _qr;

    unsigned long _lastUpdateMs;

    uint8_t _accuracy;      // Quaternion accuracy 0-3
    uint8_t _magAccuracy;   // Mag field accuracy 0-3
    uint8_t _stability;     // Stability classifier 0-4
    bool _calibrating;      // true = ground calibration mode
    bool _useGameRV;        // true = Game RV (no mag), false = standard RV

    void quaternionToEuler(float qi, float qj, float qk, float qr);
    bool enableReport();
};

#endif
