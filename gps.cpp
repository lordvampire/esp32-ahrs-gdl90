#include "gps.h"

GPSHandler::GPSHandler() {}

void GPSHandler::begin() {
    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void GPSHandler::update() {
    while (Serial2.available() > 0) {
        _gps.encode(Serial2.read());
    }
}

bool GPSHandler::isFixValid() {
    return _gps.location.isValid() && _gps.location.age() < 3000;
}

uint32_t GPSHandler::getSatellites() {
    if (_gps.satellites.isValid()) {
        return _gps.satellites.value();
    }
    return 0;
}

double GPSHandler::getLatitude() {
    if (_gps.location.isValid()) {
        return _gps.location.lat();
    }
    return 0.0;
}

double GPSHandler::getLongitude() {
    if (_gps.location.isValid()) {
        return _gps.location.lng();
    }
    return 0.0;
}

float GPSHandler::getAltitudeFt() {
    if (_gps.altitude.isValid()) {
        return (float)_gps.altitude.feet();
    }
    return 0.0f;
}

float GPSHandler::getSpeedKnots() {
    if (_gps.speed.isValid()) {
        return (float)_gps.speed.knots();
    }
    return 0.0f;
}

float GPSHandler::getTrackDeg() {
    if (_gps.course.isValid()) {
        return (float)_gps.course.deg();
    }
    return 0.0f;
}

bool GPSHandler::isTrackValid() {
    return isFixValid() &&
           _gps.speed.isValid() &&
           _gps.speed.knots() > GPS_TRACK_MIN_SPEED_KT;
}

uint32_t GPSHandler::getFixAgeMs() {
    return _gps.location.age();
}
