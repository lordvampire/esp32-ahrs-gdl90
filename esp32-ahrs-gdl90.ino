// ============================================================================
// ESP32 AHRS GDL90 — Aircraft Attitude & Heading Reference System
//
// Dual-mode operation:
//   COMPANION MODE  — auto-detected when a "stratux"/"Stratux" WiFi AP is
//                     found.  Joins the Stratux network as a client and
//                     broadcasts AHRS + Ownship (BMP390 baro + GPS) on the
//                     Stratux subnet.  Stratux provides ADS-B traffic only.
//   STANDALONE MODE — no Stratux found.  Creates own WiFi AP "AHRS-GDL90"
//                     and broadcasts everything.
//
// Port 4000:  GDL90 binary (Heartbeat, Ownship, GeoAlt, FF-ID, FF-AHRS)
// Port 49002: ForeFlight text protocol (XGPS, XATT)
//
// Hardware: ESP32-WROOM-32D, BNO086 (I2C), BMP390 (I2C), u-blox GPS (UART2)
// ============================================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "bno086.h"
#include "bmp390.h"
#include "gps.h"
#include "gdl90.h"

// ---- Operating mode --------------------------------------------------------
enum OpMode { MODE_STANDALONE, MODE_COMPANION };
OpMode opMode = MODE_STANDALONE;

// ---- WiFi AP configuration (standalone mode) --------------------------------
static const char *AP_SSID     = "AHRS-GDL90";
static const char *AP_PASSWORD = "ahrs1234";

static const IPAddress AP_IP(192, 168, 10, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// ---- WiFi Stratux connection (companion mode) -------------------------------
// Stratux AP is open (no password), DHCP assigns us an IP on 192.168.10.x
static const unsigned long WIFI_RECONNECT_MS = 10000; // retry every 10 s
unsigned long lastWiFiReconnectMs = 0;

// ---- Ports -----------------------------------------------------------------
static const uint16_t GDL90_PORT  = 4000;
static const uint16_t FF_SIM_PORT = 49002;

// ---- I2C pins (Qwiic chain) -----------------------------------------------
static const int SDA_PIN = 21;
static const int SCL_PIN = 22;

// ---- Timing ----------------------------------------------------------------
static const unsigned long GDL90_INTERVAL_MS = 1000;  // 1 Hz
static const unsigned long AHRS_INTERVAL_MS  = 200;   // 5 Hz
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

// ---- Framing buffer --------------------------------------------------------
uint8_t frameBuf[GDL90_MAX_FRAME];

// ---- Broadcast targets (filled dynamically based on mode) ------------------
static const int MAX_TARGETS = 8;
IPAddress targets[MAX_TARGETS];
int numTargets = 0;

// ---- Stratux SSID found during scan ---------------------------------------
String stratuxSSID;

// ============================================================================
// WiFi scan: look for a Stratux AP
// Returns true if found, sets stratuxSSID to the exact SSID.
// ============================================================================
bool scanForStratux() {
    Serial.println("Scanning for Stratux WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);

    int n = WiFi.scanNetworks();
    Serial.print("  Found ");
    Serial.print(n);
    Serial.println(" networks:");

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        Serial.print("    ");
        Serial.print(ssid);
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm)");

        // Case-insensitive match for "stratux"
        String lower = ssid;
        lower.toLowerCase();
        if (lower == "stratux") {
            stratuxSSID = ssid;
            Serial.print("  >>> Stratux found: \"");
            Serial.print(ssid);
            Serial.println("\"");
        }
    }

    WiFi.scanDelete();
    return stratuxSSID.length() > 0;
}

// ============================================================================
// Connect to Stratux AP (open network, no password)
// ============================================================================
bool connectToStratux() {
    Serial.print("Connecting to Stratux (\"");
    Serial.print(stratuxSSID);
    Serial.print("\")... ");

    WiFi.mode(WIFI_STA);
    WiFi.begin(stratuxSSID.c_str());

    // Wait up to 15 seconds for connection
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" OK");
        Serial.print("  IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("  Gateway: ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("  Subnet: ");
        Serial.println(WiFi.subnetMask());
        return true;
    }

    Serial.println(" FAILED");
    return false;
}

// ============================================================================
// Build the broadcast target list based on mode
// ============================================================================
void buildTargetList() {
    numTargets = 0;

    if (opMode == MODE_COMPANION) {
        // On Stratux network (192.168.10.x)
        // Broadcast to subnet + direct IPs for clients also connected
        targets[numTargets++] = IPAddress(192, 168, 10, 255);
        targets[numTargets++] = IPAddress(255, 255, 255, 255);
        // Stratux DHCP typically assigns .2 through .5
        for (int i = 2; i <= 5; i++) {
            targets[numTargets++] = IPAddress(192, 168, 10, i);
        }
    } else {
        // Own AP — same as before
        targets[numTargets++] = IPAddress(192, 168, 10, 255);
        targets[numTargets++] = IPAddress(255, 255, 255, 255);
        for (int i = 2; i <= 5; i++) {
            targets[numTargets++] = IPAddress(192, 168, 10, i);
        }
    }
}

// ============================================================================
// Helper: send a framed GDL90 buffer to all targets
// ============================================================================
void broadcastGDL90(WiFiUDP &udp, uint16_t port,
                    const uint8_t *buf, uint16_t len) {
    for (int t = 0; t < numTargets; t++) {
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
    Serial.println();

    // ---- I2C ---------------------------------------------------------------
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    // ---- BNO086 init -------------------------------------------------------
    Serial.print("Initializing BNO086... ");
    if (!imu.begin(Wire)) {
        Serial.println("FAILED — BNO086 not found. Check wiring / I2C address.");
        while (true) { delay(1000); }
    }
    Serial.println("OK");

    // ---- BMP390 init -------------------------------------------------------
    Serial.print("Initializing BMP390... ");
    if (!baro.begin(Wire)) {
        Serial.println("FAILED — BMP390 not found. Check wiring / I2C address.");
        while (true) { delay(1000); }
    }
    Serial.println("OK");

    // ---- GPS init ----------------------------------------------------------
    Serial.print("Initializing GPS (UART2)... ");
    gps.begin();
    Serial.println("OK (waiting for fix)");

    // ---- WiFi mode detection -----------------------------------------------
    Serial.println();
    if (scanForStratux()) {
        // ---- COMPANION MODE ------------------------------------------------
        if (connectToStratux()) {
            opMode = MODE_COMPANION;
            Serial.println();
            Serial.println(">>> COMPANION MODE <<<");
            Serial.println("    AHRS + Ownship (BMP390/GPS) on Stratux network");
            Serial.println("    Stratux provides ADS-B traffic");
        } else {
            Serial.println("Stratux connection failed, falling back to Standalone.");
            opMode = MODE_STANDALONE;
        }
    }

    if (opMode == MODE_STANDALONE) {
        // ---- STANDALONE MODE -----------------------------------------------
        Serial.println();
        Serial.println(">>> STANDALONE MODE <<<");
        Serial.print("Starting WiFi AP... ");
        WiFi.mode(WIFI_AP);
        delay(100);
        WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
        delay(100);
        WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
        delay(500);
        Serial.print("SSID: ");
        Serial.print(AP_SSID);
        Serial.print("  IP: ");
        Serial.println(WiFi.softAPIP());
    }

    // ---- Build target list -------------------------------------------------
    buildTargetList();

    // ---- UDP sockets -------------------------------------------------------
    udpGDL90.begin(GDL90_PORT);
    Serial.print("GDL90 binary on port ");
    Serial.println(GDL90_PORT);

    udpFFSim.begin(FF_SIM_PORT);
    Serial.print("ForeFlight XGPS/XATT on port ");
    Serial.println(FF_SIM_PORT);

    Serial.println();
    Serial.println("Setup complete. Running...");
    Serial.println();

    lastGDL90Ms = millis();
    lastAHRSMs  = millis();
    lastDebugMs = millis();
}

// ============================================================================
// loop()
// ============================================================================
void loop() {
    unsigned long now = millis();

    // ---- Companion mode: check WiFi connection, reconnect if lost ----------
    if (opMode == MODE_COMPANION && WiFi.status() != WL_CONNECTED) {
        if (now - lastWiFiReconnectMs >= WIFI_RECONNECT_MS) {
            lastWiFiReconnectMs = now;
            Serial.println("WARNING: Stratux WiFi lost, reconnecting...");
            connectToStratux();
        }
        // Don't send data while disconnected
        imu.update();
        gps.update();
        return;
    }

    // ---- 1. Read IMU (non-blocking) ----------------------------------------
    imu.update();

    // ---- Watchdog: re-init BNO086 if stale ---------------------------------
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

    // ---- 3. Determine heading ----------------------------------------------
    float heading;
    if (gps.isTrackValid()) {
        heading = gps.getTrackDeg();
    } else {
        heading = imu.getYaw();
    }

    // ---- 4. 5 Hz: ForeFlight AHRS + XATT ----------------------------------
    if (now - lastAHRSMs >= AHRS_INTERVAL_MS) {
        lastAHRSMs = now;

        float roll  = imu.getRoll();
        float pitch = imu.getPitch();

        // ForeFlight binary AHRS (0x65 sub 0x01) on port 4000
        uint16_t ahrsLen = gdl90.buildForeFlightAHRS(frameBuf, roll, pitch, heading);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ahrsLen);

        // ForeFlight text XATT on port 49002
        for (int t = 0; t < numTargets; t++) {
            gdl90.sendXATT(udpFFSim, targets[t], FF_SIM_PORT,
                           heading, pitch, roll);
        }
    }

    // ---- 5. 1 Hz: GDL90 messages -------------------------------------------
    if (now - lastGDL90Ms >= GDL90_INTERVAL_MS) {
        lastGDL90Ms = now;

        // Read barometer
        baro.update();

        // NIC/NACp
        uint8_t nic  = gps.isFixValid() ? 8 : 0;
        uint8_t nacp = gps.isFixValid() ? 8 : 0;

        // Pressure altitude from BMP390
        int32_t pressAltFt = (int32_t)baro.getPressureAltFt();

        // GPS-derived values
        double lat     = gps.getLatitude();
        double lon     = gps.getLongitude();
        float  speedKt = gps.getSpeedKnots();
        float  track   = gps.isTrackValid() ? gps.getTrackDeg() : heading;
        int32_t geoAlt = (int32_t)gps.getAltitudeFt();

        // Heartbeat (0x00)
        uint16_t hbLen = gdl90.buildHeartbeat(frameBuf);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, hbLen);

        // ForeFlight Device ID (0x65 sub 0x00)
        uint16_t ffLen = gdl90.buildForeFlightID(frameBuf);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ffLen);

        // Ownship Report (0x0A) — always from ESP32 (BMP390 baro + GPS)
        uint16_t osLen = gdl90.buildOwnshipReport(frameBuf,
                                        lat, lon,
                                        pressAltFt,
                                        track, speedKt,
                                        nic, nacp);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, osLen);

        // Ownship Geometric Altitude (0x0B)
        uint16_t gaLen = gdl90.buildOwnshipGeoAlt(frameBuf, geoAlt);
        broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, gaLen);

        // XGPS on port 49002
        float altMslM = gps.getAltitudeFt() * 0.3048f;
        float gsMs    = speedKt * 0.514444f;
        for (int t = 0; t < numTargets; t++) {
            gdl90.sendXGPS(udpFFSim, targets[t], FF_SIM_PORT,
                           lat, lon, altMslM, track, gsMs);
        }
    }

    // ---- 6. Serial debug output (every 2 seconds) -------------------------
    if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
        lastDebugMs = now;

        // Mode indicator
        if (opMode == MODE_COMPANION) {
            Serial.print("[COMPANION] ");
        } else {
            Serial.print("[STANDALONE] ");
        }

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

        if (opMode == MODE_STANDALONE) {
            Serial.print("  Clients: ");
            Serial.print(WiFi.softAPgetStationNum());
        } else {
            Serial.print("  WiFi: ");
            Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "LOST");
            Serial.print("  RSSI: ");
            Serial.print(WiFi.RSSI());
        }

        Serial.println();
    }
}
