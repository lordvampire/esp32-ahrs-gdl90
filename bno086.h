#ifndef BNO086_H
#define BNO086_H

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

// I2C address for BNO086 on Qwiic chain
#define BNO086_ADDR 0x4B

// Report interval in milliseconds (100ms = 10Hz)
#define BNO086_REPORT_INTERVAL_MS 100

// Watchdog timeout: re-init if no data for this many milliseconds
#define BNO086_WATCHDOG_MS 2000

class BNO086Handler {
public:
    BNO086Handler();

    // Initialize the sensor. Returns true on success.
    bool begin(TwoWire &wire = Wire);

    // Call in loop. Returns true if new data was read.
    bool update();

    // Attempt to re-initialize (for watchdog recovery).
    bool reinit();

    // Attitude in degrees
    float getRoll() const;   // positive = right wing down
    float getPitch() const;  // positive = nose up
    float getYaw() const;    // 0-360 magnetic/game heading

    // Raw quaternion components
    float getQI() const;
    float getQJ() const;
    float getQK() const;
    float getQReal() const;

    // Timestamp of last successful read (millis)
    unsigned long getLastUpdateMs() const;

    // Check if watchdog expired (no data for BNO086_WATCHDOG_MS)
    bool isWatchdogExpired() const;

private:
    BNO08x _imu;
    TwoWire *_wire;

    float _roll;
    float _pitch;
    float _yaw;
    float _qi, _qj, _qk, _qr;

    unsigned long _lastUpdateMs;

    void quaternionToEuler(float qi, float qj, float qk, float qr);
    bool enableReport();
};

#endif
