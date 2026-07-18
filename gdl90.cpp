#include "gdl90.h"

// --------------------------------------------------------------------------
// GDL90 CRC-16 table
// Standard (non-reflected) CRC-CCITT poly 0x1021 as specified in
// FAA GDL90 ICD Appendix A and used by Stratux.
// --------------------------------------------------------------------------
const uint16_t GDL90::crcTable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

GDL90::GDL90() {}

// --------------------------------------------------------------------------
// CRC-16 (standard / non-reflected CRC-CCITT)
// Per GDL90 ICD Appendix A: crc = table[crc>>8] ^ (crc<<8) ^ byte
// --------------------------------------------------------------------------
uint16_t GDL90::crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0x0000;
    for (uint16_t i = 0; i < len; i++) {
        crc = crcTable[crc >> 8] ^ (crc << 8) ^ (uint16_t)data[i];
    }
    return crc;
}

// --------------------------------------------------------------------------
// Frame a raw message: flag + escaped(msg + CRC_lo + CRC_hi) + flag
// --------------------------------------------------------------------------
uint16_t GDL90::frame(const uint8_t *raw, uint16_t rawLen, uint8_t *frameBuf) {
    // Compute CRC over the raw message bytes (ID + payload).
    uint16_t crc = crc16(raw, rawLen);

    // Build the data to be escaped: raw message + CRC low + CRC high
    uint8_t temp[GDL90_MAX_RAW + 2];
    memcpy(temp, raw, rawLen);
    temp[rawLen]     = (uint8_t)(crc & 0xFF);        // CRC low byte first
    temp[rawLen + 1] = (uint8_t)((crc >> 8) & 0xFF); // CRC high byte
    uint16_t totalLen = rawLen + 2;

    // Frame: opening flag
    uint16_t idx = 0;
    frameBuf[idx++] = GDL90_FLAG;

    // Byte-stuff the payload + CRC
    for (uint16_t i = 0; i < totalLen; i++) {
        uint8_t b = temp[i];
        if (b == GDL90_FLAG || b == GDL90_ESC) {
            frameBuf[idx++] = GDL90_ESC;
            frameBuf[idx++] = b ^ GDL90_XOR;
        } else {
            frameBuf[idx++] = b;
        }
    }

    // Closing flag
    frameBuf[idx++] = GDL90_FLAG;
    return idx;
}

// --------------------------------------------------------------------------
// Heartbeat (Message ID 0x00) - 7 raw bytes
// Per GDL90 ICD section 3.1.
// --------------------------------------------------------------------------
uint16_t GDL90::buildHeartbeat(uint8_t *frameBuf) {
    uint8_t raw[7];
    raw[0] = GDL90_MSG_HEARTBEAT;

    // Status byte 1: bit0 = GPS pos valid, bit7 = UAT initialized
    raw[1] = 0x81; // UAT initialized, GPS pos valid

    // Status byte 2: bit0 = UTC OK (timestamp valid)
    raw[2] = 0x01;

    // Timestamp (seconds since UTC midnight, lower 16 bits).
    // We don't have reliable UTC, send 0xFFFF.
    raw[3] = 0xFF;
    raw[4] = 0xFF;

    // Message counts
    raw[5] = 0x00;
    raw[6] = 0x00;

    return frame(raw, 7, frameBuf);
}

// --------------------------------------------------------------------------
// Ownship Report (Message ID 0x0A) - 28 raw bytes
// Per GDL90 ICD section 3.5.
// --------------------------------------------------------------------------
uint16_t GDL90::buildOwnshipReport(uint8_t *frameBuf,
                                    double lat, double lon,
                                    int32_t altFt,
                                    float trackDeg, float speedKt,
                                    uint8_t nic, uint8_t nacp) {
    uint8_t raw[28];
    memset(raw, 0, sizeof(raw));
    raw[0] = GDL90_MSG_OWNSHIP_REPORT;

    // Byte 1: Alert status (upper nibble) | Address type (lower nibble)
    raw[1] = 0x00;

    // Bytes 2-4: Participant address (self-assigned)
    raw[2] = 0x00;
    raw[3] = 0x00;
    raw[4] = 0x01;

    // Bytes 5-7: Latitude (24-bit signed, semicircles)
    int32_t lat24 = (int32_t)(lat * (8388608.0 / 180.0));
    raw[5] = (uint8_t)((lat24 >> 16) & 0xFF);
    raw[6] = (uint8_t)((lat24 >> 8) & 0xFF);
    raw[7] = (uint8_t)(lat24 & 0xFF);

    // Bytes 8-10: Longitude (24-bit signed, semicircles)
    int32_t lon24 = (int32_t)(lon * (8388608.0 / 180.0));
    raw[8]  = (uint8_t)((lon24 >> 16) & 0xFF);
    raw[9]  = (uint8_t)((lon24 >> 8) & 0xFF);
    raw[10] = (uint8_t)(lon24 & 0xFF);

    // Bytes 11-12: Altitude (12 bits) + Misc (4 bits)
    int32_t altEnc = (altFt + 1000) / 25;
    if (altEnc < 0) altEnc = 0;
    if (altEnc > 0xFFF) altEnc = 0xFFF;
    uint8_t misc = 0x09; // airborne + true track
    raw[11] = (uint8_t)((altEnc >> 4) & 0xFF);
    raw[12] = (uint8_t)(((altEnc & 0x0F) << 4) | (misc & 0x0F));

    // Byte 13: NIC (upper nibble) | NACp (lower nibble)
    raw[13] = (uint8_t)(((nic & 0x0F) << 4) | (nacp & 0x0F));

    // Bytes 14-16: Horizontal velocity (12 bits) | Vertical velocity (12 bits)
    uint16_t hVel;
    if (speedKt < 0) {
        hVel = 0xFFF;
    } else {
        hVel = (uint16_t)(speedKt + 0.5f);
        if (hVel > 0xFFE) hVel = 0xFFE;
    }
    uint16_t vVel = 0x800; // not available
    raw[14] = (uint8_t)((hVel >> 4) & 0xFF);
    raw[15] = (uint8_t)(((hVel & 0x0F) << 4) | ((vVel >> 8) & 0x0F));
    raw[16] = (uint8_t)(vVel & 0xFF);

    // Byte 17: Track/Heading (0-360 mapped to 0-255)
    float trackNorm = trackDeg;
    if (trackNorm < 0.0f) trackNorm += 360.0f;
    if (trackNorm >= 360.0f) trackNorm -= 360.0f;
    raw[17] = (uint8_t)((trackNorm / 360.0f) * 256.0f);

    // Byte 18: Emitter category = 1 (light aircraft < 15500 lbs)
    raw[18] = 0x01;

    // Bytes 19-26: Callsign (8 ASCII chars, space-padded)
    const char *cs = "N000000 ";
    for (int i = 0; i < 8; i++) {
        raw[19 + i] = (uint8_t)cs[i];
    }

    // Byte 27: Emergency/priority code
    raw[27] = 0x00;

    return frame(raw, 28, frameBuf);
}

// --------------------------------------------------------------------------
// Ownship Geometric Altitude (Message ID 0x0B) - 5 raw bytes
// Per GDL90 ICD section 3.6.
// --------------------------------------------------------------------------
uint16_t GDL90::buildOwnshipGeoAlt(uint8_t *frameBuf, int32_t geoAltFt) {
    uint8_t raw[5];
    raw[0] = GDL90_MSG_OWNSHIP_GEO_ALT;

    // Geometric altitude: signed 16-bit, units of 5 feet
    int16_t geoEnc = (int16_t)(geoAltFt / 5);
    raw[1] = (uint8_t)((geoEnc >> 8) & 0xFF);
    raw[2] = (uint8_t)(geoEnc & 0xFF);

    // VFOM: 0x7FFF = not available
    raw[3] = 0x7F;
    raw[4] = 0xFF;

    return frame(raw, 5, frameBuf);
}

// --------------------------------------------------------------------------
// ForeFlight Device ID (Message ID 0x65, Sub-ID 0x00) - 39 raw bytes
// Per ForeFlight GDL90 Extended Specification and Stratux implementation.
//
// Layout:
//   [0]      0x65        Message ID
//   [1]      0x00        Sub-ID (device ID)
//   [2]      0x01        Version
//   [3-10]   serial      8 bytes (0xFF = invalid)
//   [11-18]  short name  8 bytes UTF-8, null-padded
//   [19-34]  long name   16 bytes UTF-8, null-padded
//   [35-38]  caps mask   4 bytes (0 = WGS-84/HAE, unrestricted internet)
// --------------------------------------------------------------------------
uint16_t GDL90::buildForeFlightID(uint8_t *frameBuf) {
    uint8_t raw[39];
    memset(raw, 0, sizeof(raw));

    raw[0] = GDL90_MSG_FOREFLIGHT; // 0x65
    raw[1] = FF_SUB_ID;            // 0x00
    raw[2] = 0x01;                 // version

    // Serial number: 0xFF = invalid (same as Stratux)
    for (int i = 3; i <= 10; i++) {
        raw[i] = 0xFF;
    }

    // Short name (8 bytes, null-padded)
    const char *shortName = "AHRS-GDL";
    for (int i = 0; i < 8 && shortName[i] != '\0'; i++) {
        raw[11 + i] = (uint8_t)shortName[i];
    }

    // Long name (16 bytes, null-padded)
    const char *longName = "ESP32-AHRS-GDL90";
    for (int i = 0; i < 16 && longName[i] != '\0'; i++) {
        raw[19 + i] = (uint8_t)longName[i];
    }

    // Capabilities mask (4 bytes, all zero)
    // Bit 0 = 0: WGS-84 ellipsoid (HAE) for geometric altitude
    // Bits 1-2 = 0: unrestricted internet
    raw[35] = 0x00;
    raw[36] = 0x00;
    raw[37] = 0x00;
    raw[38] = 0x00;

    return frame(raw, 39, frameBuf);
}

// --------------------------------------------------------------------------
// ForeFlight AHRS (Message ID 0x65, Sub-ID 0x01) - 12 raw bytes
// Per ForeFlight GDL90 Extended Specification. Send at 5 Hz.
//
// Layout:
//   [0]      0x65        Message ID
//   [1]      0x01        Sub-ID (AHRS)
//   [2-3]    roll        int16 big-endian, units of 1/10 degree
//   [4-5]    pitch       int16 big-endian, units of 1/10 degree
//   [6-7]    heading     uint16 big-endian, units of 1/10 degree
//                        bit 15: 0 = true heading, 1 = magnetic
//   [8-9]    IAS         uint16 big-endian, knots (0xFFFF = invalid)
//   [10-11]  TAS         uint16 big-endian, knots (0xFFFF = invalid)
// --------------------------------------------------------------------------
uint16_t GDL90::buildForeFlightAHRS(uint8_t *frameBuf,
                                     float rollDeg, float pitchDeg,
                                     float headingDeg) {
    uint8_t raw[12];

    raw[0] = GDL90_MSG_FOREFLIGHT; // 0x65
    raw[1] = FF_SUB_AHRS;          // 0x01

    // Roll: 1/10 degree, int16 big-endian
    int16_t roll10 = (int16_t)(rollDeg * 10.0f);
    raw[2] = (uint8_t)((roll10 >> 8) & 0xFF);
    raw[3] = (uint8_t)(roll10 & 0xFF);

    // Pitch: 1/10 degree, int16 big-endian
    int16_t pitch10 = (int16_t)(pitchDeg * 10.0f);
    raw[4] = (uint8_t)((pitch10 >> 8) & 0xFF);
    raw[5] = (uint8_t)(pitch10 & 0xFF);

    // Heading: 1/10 degree, uint16 big-endian, bit 15 = 0 (true heading)
    float hdgNorm = headingDeg;
    if (hdgNorm < 0.0f) hdgNorm += 360.0f;
    if (hdgNorm >= 360.0f) hdgNorm -= 360.0f;
    uint16_t hdg10 = (uint16_t)(hdgNorm * 10.0f);
    raw[6] = (uint8_t)((hdg10 >> 8) & 0xFF);
    raw[7] = (uint8_t)(hdg10 & 0xFF);

    // IAS: not available
    raw[8]  = 0xFF;
    raw[9]  = 0xFF;

    // TAS: not available
    raw[10] = 0xFF;
    raw[11] = 0xFF;

    return frame(raw, 12, frameBuf);
}

// --------------------------------------------------------------------------
// Send a framed buffer via UDP.
// --------------------------------------------------------------------------
void GDL90::sendUDP(WiFiUDP &udp, IPAddress ip, uint16_t port,
                    const uint8_t *frameBuf, uint16_t len) {
    udp.beginPacket(ip, port);
    udp.write(frameBuf, len);
    udp.endPacket();
}

// --------------------------------------------------------------------------
// Send XGPS sentence (ForeFlight simulator GPS protocol, port 49002).
// Format: "XGPSDeviceName,lon,lat,altMSL_m,track_deg,gs_m_s"
// --------------------------------------------------------------------------
void GDL90::sendXGPS(WiFiUDP &udp, IPAddress ip, uint16_t port,
                     double lat, double lon, float altMslM,
                     float trackDeg, float gsMs) {
    char buf[128];
    snprintf(buf, sizeof(buf), "XGPSAHRS-GDL,%f,%f,%.1f,%.1f,%.1f",
             lon, lat, altMslM, trackDeg, gsMs);

    udp.beginPacket(ip, port);
    udp.write((const uint8_t *)buf, strlen(buf));
    udp.endPacket();
}

// --------------------------------------------------------------------------
// Send XATT sentence (ForeFlight simulator AHRS protocol, port 49002).
// Format: "XATTDeviceName,heading,pitch,roll"
// --------------------------------------------------------------------------
void GDL90::sendXATT(WiFiUDP &udp, IPAddress ip, uint16_t port,
                     float headingDeg, float pitchDeg, float rollDeg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "XATTAHRS-GDL,%.1f,%.1f,%.1f",
             headingDeg, pitchDeg, rollDeg);

    udp.beginPacket(ip, port);
    udp.write((const uint8_t *)buf, strlen(buf));
    udp.endPacket();
}
