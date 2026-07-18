#ifndef GDL90_H
#define GDL90_H

#include <Arduino.h>
#include <WiFiUdp.h>

// GDL90 flag and escape bytes
#define GDL90_FLAG  0x7E
#define GDL90_ESC   0x7D
#define GDL90_XOR   0x20

// Message IDs
#define GDL90_MSG_HEARTBEAT         0x00
#define GDL90_MSG_OWNSHIP_REPORT    0x0A
#define GDL90_MSG_OWNSHIP_GEO_ALT   0x0B
#define GDL90_MSG_FOREFLIGHT        0x65

// ForeFlight sub-IDs
#define FF_SUB_ID       0x00   // Device ID message
#define FF_SUB_AHRS     0x01   // AHRS attitude message

// Max raw message size before framing (ForeFlight ID is 39 bytes)
#define GDL90_MAX_RAW  64
// Max framed message size (worst case: every byte escaped + 2 flags)
#define GDL90_MAX_FRAME 140

class GDL90 {
public:
    GDL90();

    // Build a Heartbeat message (ID 0x00). Returns framed length.
    uint16_t buildHeartbeat(uint8_t *frameBuf);

    // Build an Ownship Report (ID 0x0A). Returns framed length.
    uint16_t buildOwnshipReport(uint8_t *frameBuf,
                                double lat, double lon,
                                int32_t altFt,
                                float trackDeg, float speedKt,
                                uint8_t nic, uint8_t nacp);

    // Build Ownship Geometric Altitude (ID 0x0B). Returns framed length.
    uint16_t buildOwnshipGeoAlt(uint8_t *frameBuf, int32_t geoAltFt);

    // Build ForeFlight Device ID message (ID 0x65, sub 0x00).
    // 39-byte message per ForeFlight GDL90 Extended Specification.
    uint16_t buildForeFlightID(uint8_t *frameBuf);

    // Build ForeFlight AHRS message (ID 0x65, sub 0x01).
    // 12-byte binary attitude message, should be sent at 5 Hz.
    uint16_t buildForeFlightAHRS(uint8_t *frameBuf,
                                  float rollDeg, float pitchDeg,
                                  float headingDeg);

    // Send a pre-framed buffer via UDP.
    void sendUDP(WiFiUDP &udp, IPAddress ip, uint16_t port,
                 const uint8_t *frameBuf, uint16_t len);

    // Send XGPS sentence on port 49002 (ForeFlight simulator GPS protocol).
    // Format: "XGPSDeviceName,lon,lat,altMSL_m,track_deg,gs_m_s"
    void sendXGPS(WiFiUDP &udp, IPAddress ip, uint16_t port,
                  double lat, double lon, float altMslM,
                  float trackDeg, float gsMs);

    // Send XATT sentence on port 49002 (ForeFlight simulator AHRS protocol).
    // Format: "XATTDeviceName,heading,pitch,roll"
    void sendXATT(WiFiUDP &udp, IPAddress ip, uint16_t port,
                  float headingDeg, float pitchDeg, float rollDeg);

private:
    // CRC-16 per GDL90 ICD (standard CRC-CCITT, poly 0x1021).
    uint16_t crc16(const uint8_t *data, uint16_t len);

    // Frame a raw message: flag + escaped(msg + CRC_lo + CRC_hi) + flag
    uint16_t frame(const uint8_t *raw, uint16_t rawLen, uint8_t *frameBuf);

    // Pre-computed CRC table (256 entries).
    static const uint16_t crcTable[256];
};

#endif
