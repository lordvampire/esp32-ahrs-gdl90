// ============================================================================
// ESP32 AHRS GDL90 — Aircraft Attitude & Heading Reference System
//
// Transmits GDL90 binary messages and ForeFlight attitude data via WiFi UDP
// for synthetic vision and attitude display.
//
// Port 4000:  GDL90 binary (Heartbeat, Ownship, GeoAlt, FF-ID, FF-AHRS)
// Port 49002: ForeFlight text protocol (XGPS, XATT)
//
// Hardware: NodeMCU ESP-32S, BNO086 (I2C), BMP390 (I2C), u-blox GPS (UART2)
// ============================================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "bno086.h"
#include "bmp390.h"
#include "gps.h"
#include "gdl90.h"

// ---- WiFi AP configuration ------------------------------------------------
static const char *AP_SSID     = "AHRS-GDL90";
static const char *AP_PASSWORD = "ahrs1234";

static const IPAddress AP_IP(192, 168, 10, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);
static const IPAddress BROADCAST_IP(192, 168, 10, 255);
static const uint16_t  GDL90_PORT = 4000;   // GDL90 binary messages
static const uint16_t  FF_SIM_PORT = 49002;  // ForeFlight XGPS/XATT text

// ---- I2C pins (Qwiic chain) -----------------------------------------------
static const int SDA_PIN = 21;
static const int SCL_PIN = 22;

// ---- Timing ----------------------------------------------------------------
static const unsigned long GDL90_INTERVAL_MS = 1000;  // 1 Hz: HB, Ownship, GeoAlt, FF-ID
static const unsigned long AHRS_INTERVAL_MS  = 200;   // 5 Hz: FF-AHRS + XATT
static const unsigned long DEBUG_INTERVAL_MS = 2000;   // 2 s serial debug

// ---- Global objects --------------------------------------------------------
BNO086Handler imu;
BMP390Handler baro;
GPSHandler    gps;
GDL90         gdl90;
WiFiUDP       udpGDL90;     // port 4000
WiFiUDP       udpFFSim;     // port 49002

// ---- Timing state ----------------------------------------------------------
unsigned long lastGDL90Ms  = 0;
unsigned long lastAHRSMs   = 0;
unsigned long lastDebugMs  = 0;

// ---- Framing buffers -------------------------------------------------------
uint8_t frameBuf[GDL90_MAX_FRAME];

// ---- Target IPs (broadcast + possible DHCP clients) ------------------------
static const IPAddress targets[] = {
    IPAddress(192, 168, 10, 255),   // subnet broadcast
    IPAddress(255, 255, 255, 255),  // global broadcast
    IPAddress(192, 168, 10, 2),     // client 1
    IPAddress(192, 168, 10, 3),     // client 2
    IPAddress(192, 168, 10, 4),     // client 3
    IPAddress(192, 168, 10, 5),     // client 4
};
static const int NUM_TARGETS = sizeof(targets) / sizeof(targets[0]);

// ============================================================================
// Helper: send a framed GDL90 buffer to all targets on a given port
// ============================================================================
void broadcastGDL90(WiFiUDP &udp, uint16_t port,
                    const uint8_t *buf, uint16_t len) {
    for (int t = 0; t < NUM_TARGETS; t++) {
        gdl90.sendUDP(udp, targets[t], port, buf, len);
    }
}

// ============================================================================
// setup()
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== ESP32 AHRS GDL90 ===");

    // ---- I2C ---------------------------------------------------------------
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000); // 400 kHz fast mode

    // ---- BNO086 init -------------------------------------------------------
    Serial.print("Initializing BNO086... ");
    if (!imu.begin(Wire)) {
        Serial.println("FAILED — BNO086 not found. Check wiring / I2C address.");
        while (true) { delay(1000); } // halt
    }
    Serial.println("OK");

    // ---- BMP390 init -------------------------------------------------------
    Serial.print("Initializing BMP390... ");
    if (!baro.begin(Wire)) {
        Serial.println("FAILED — BMP390 not found. Check wiring / I2C address.");
        while (true) { delay(1000); } // halt
    }
    Serial.println("OK");

    // ---- GPS init ----------------------------------------------------------
    Serial.print("Initializing GPS (UART2)... ");
    gps.begin();
    Serial.println("OK (waiting for fix)");

    // ---- WiFi AP -----------------------------------------------------------
    Serial.print("Starting WiFi AP... ");
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
    delay(100);
    WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);  // channel 1, not hidden, max 4 clients
    delay(500);
    Serial.print("SSID: ");
    Serial.print(AP_SSID);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("WiFi channel: ");
    Serial.println(WiFi.channel());

    // ---- UDP sockets -------------------------------------------------------
    udpGDL90.begin(GDL90_PORT);
    Serial.print("GDL90 binary on port ");
    Serial.println(GDL90_PORT);

    udpFFSim.begin(FF_SIM_PORT);
    Serial.print("ForeFlight XGPS/XATT on port ");
    Serial.println(FF_SIM_PORT);

    Serial.println("Setup complete. Running...\n");

    lastGDL90Ms = millis();
    lastAHRSMs  = millis();
    lastDebugMs = millis();
}

// ============================================================================
// loop()
// ============================================================================
void loop() {
    unsigned long now = millis();

    // ---- 1. Read IMU (non-blocking, 10Hz reports from BNO086) --------------
    imu.update();

    // ---- Watchdog: re-init BNO086 if stale (max once per 10 seconds) --------
    static unsigned long lastWatchdogAttempt = 0;
    if (imu.isWatchdogExpired() && (now - lastWatchdogAttempt > 10000)) {
        lastWatchdogAttempt = now;
        Serial.println("WARNING: BNO086 watchdog expired, attempting re-init...");
        if (imu.reinit()) {
            Serial.println("BNO086 re-initialized OK");
        } else {
            Serial.println("BNO086 re-init FAILED");
        }
    }

    // ---- 2. Read GPS NMEA (non-blocking) -----------------------------------
    gps.update();

    // ---- 3. Determine heading (shared by 1 Hz and 5 Hz paths) ---------------
    float heading;
    if (gps.isTrackValid()) {
        heading = gps.getTrackDeg();
    } else {
        heading = imu.getYaw();
    }

    // ---- 4. 5 Hz ForeFlight AHRS (binary on port 4000 + XATT on 49002) -----
    if (now - lastAHRSMs >= AHRS_INTERVAL_MS) {
        lastAHRSMs = now;

        float roll  = imu.getRoll();
        float pitch = imu.getPitch();

        // ForeFlight binary AHRS (0x65 sub 0x01) on port 4000
        uint16_t ahrsLen = gdl90.buildForeFlightAHRS(frameBuf, roll, pitch, heading);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ahrsLen);

        // ForeFlight text XATT on port 49002
        for (int t = 0; t < NUM_TARGETS; t++) {
            gdl90.sendXATT(udpFFSim, targets[t], FF_SIM_PORT,
                           heading, pitch, roll);
        }
    }

    // ---- 5. 1 Hz GDL90 broadcast -------------------------------------------
    if (now - lastGDL90Ms >= GDL90_INTERVAL_MS) {
        lastGDL90Ms = now;

        // Read barometer
        baro.update();

        // Determine NIC/NACp
        uint8_t nic  = gps.isFixValid() ? 8 : 0;
        uint8_t nacp = gps.isFixValid() ? 8 : 0;

        // Pressure altitude from barometer
        int32_t pressAltFt = (int32_t)baro.getPressureAltFt();

        // GPS-derived values
        double lat     = gps.getLatitude();
        double lon     = gps.getLongitude();
        float  speedKt = gps.getSpeedKnots();
        float  track   = gps.isTrackValid() ? gps.getTrackDeg() : heading;
        int32_t geoAlt = (int32_t)gps.getAltitudeFt();

        // --- Build and send all 1 Hz messages ---

        // Heartbeat (0x00)
        uint16_t hbLen = gdl90.buildHeartbeat(frameBuf);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, hbLen);

        // ForeFlight Device ID (0x65 sub 0x00)
        uint16_t ffLen = gdl90.buildForeFlightID(frameBuf);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ffLen);

        // Ownship Report (0x0A)
        uint16_t osLen = gdl90.buildOwnshipReport(frameBuf,
                                        lat, lon,
                                        pressAltFt,
                                        track, speedKt,
                                        nic, nacp);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, osLen);

        // Ownship Geometric Altitude (0x0B)
        uint16_t gaLen = gdl90.buildOwnshipGeoAlt(frameBuf, geoAlt);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, gaLen);

        // XGPS on port 49002 (altitude in meters, speed in m/s)
        float altMslM = gps.getAltitudeFt() * 0.3048f;
        float gsMs    = speedKt * 0.514444f;
        for (int t = 0; t < NUM_TARGETS; t++) {
            gdl90.sendXGPS(udpFFSim, targets[t], FF_SIM_PORT,
                           lat, lon, altMslM, track, gsMs);
        }
    }

    // ---- 6. Serial debug output (every 2 seconds) -------------------------
    if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
        lastDebugMs = now;

        Serial.print("Roll: ");
        Serial.print(imu.getRoll(), 1);
        Serial.print("  Pitch: ");
        Serial.print(imu.getPitch(), 1);
        Serial.print("  Hdg: ");
        if (gps.isTrackValid()) {
            Serial.print(gps.getTrackDeg(), 1);
            Serial.print(" (GPS)");
        } else {
            Serial.print(imu.getYaw(), 1);
            Serial.print(" (IMU)");
        }

        Serial.print("  PressAlt: ");
        Serial.print(baro.getPressureAltFt(), 0);
        Serial.print(" ft");

        Serial.print("  GPS: ");
        if (gps.isFixValid()) {
            Serial.print("FIX");
        } else {
            Serial.print("NO FIX");
        }
        Serial.print("  Sats: ");
        Serial.print(gps.getSatellites());

        Serial.print("  Clients: ");
        Serial.print(WiFi.softAPgetStationNum());

        Serial.println();
    }
}
