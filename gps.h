#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPSPlus.h>

// UART2 pin definitions for ESP32
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_BAUD   9600

// Minimum ground speed (knots) before GPS track overrides IMU heading
#define GPS_TRACK_MIN_SPEED_KT 5.0

class GPSHandler {
public:
    GPSHandler();

    // Initialize Serial2 for GPS.
    void begin();

    // Feed incoming bytes to TinyGPSPlus (non-blocking). Call every loop.
    void update();

    // Fix status
    bool isFixValid();
    uint32_t getSatellites();

    // Position
    double getLatitude();   // degrees
    double getLongitude();  // degrees

    // Altitude from GPS in feet
    float getAltitudeFt();

    // Ground speed in knots
    float getSpeedKnots();

    // Ground track (course over ground) in degrees true
    float getTrackDeg();

    // Whether GPS track should be used as heading
    // (fix valid AND speed above threshold)
    bool isTrackValid();

    // Age of last valid position fix in milliseconds
    uint32_t getFixAgeMs();

private:
    TinyGPSPlus _gps;
};

#endif
