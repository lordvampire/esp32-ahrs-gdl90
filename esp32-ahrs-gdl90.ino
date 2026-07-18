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
#include <WebServer.h>

#include "bno086.h"
#include "bmp390.h"
#include "gps.h"
#include "gdl90.h"
#include "config.h"
#include "webui.h"

// ---- Operating mode --------------------------------------------------------
enum OpMode { MODE_STANDALONE, MODE_COMPANION };
OpMode opMode = MODE_STANDALONE;

// ---- Runtime config (loaded from NVS) --------------------------------------
Config config;

// ---- WiFi AP defaults (used if config values are applied) -------------------
static const IPAddress AP_IP(192, 168, 10, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// ---- WebServer on port 80 --------------------------------------------------
WebServer server(80);

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

// ---- Runtime sensor status (true = initialized successfully) ---------------
bool bno086ok = false;
bool bmp390ok = false;

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

        // Case-insensitive match for configured Stratux SSID
        String lower = ssid;
        lower.toLowerCase();
        String target = String(config.stratuxSSID);
        target.toLowerCase();
        if (lower == target) {
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
// WebServer handlers
// ============================================================================
void handleRoot() {
    server.send_P(200, "text/html", WEBUI_HTML);
}

void handleGetStatus() {
    float roll  = bno086ok ? (config.invertRoll ? -imu.getRoll() : imu.getRoll()) : 0;
    float pitch = bno086ok ? imu.getPitch() : 0;
    float heading;
    const char *hdgSrc;
    if (config.enableGPS && gps.isTrackValid()) {
        heading = gps.getTrackDeg();
        hdgSrc = "GPS";
    } else if (bno086ok) {
        heading = imu.getYaw();
        hdgSrc = "IMU";
    } else {
        heading = 0;
        hdgSrc = "NONE";
    }

    char json[448];
    snprintf(json, sizeof(json),
        "{\"mode\":\"%s\","
        "\"roll\":%.1f,\"pitch\":%.1f,\"heading\":%.1f,"
        "\"headingSource\":\"%s\","
        "\"pressAltFt\":%d,"
        "\"gpsFix\":%s,\"gpsEnabled\":%s,\"satellites\":%u,"
        "\"wifiRSSI\":%d,\"clients\":%d,"
        "\"uptimeMs\":%lu,"
        "\"bno086ok\":%s,\"bmp390ok\":%s}",
        opMode == MODE_COMPANION ? "COMPANION" : "STANDALONE",
        roll, pitch, heading,
        hdgSrc,
        bmp390ok ? (int)baro.getPressureAltFt() : 0,
        (config.enableGPS && gps.isFixValid()) ? "true" : "false",
        config.enableGPS ? "true" : "false",
        config.enableGPS ? gps.getSatellites() : 0,
        (int)WiFi.RSSI(),
        opMode == MODE_STANDALONE ? (int)WiFi.softAPgetStationNum() : 0,
        millis(),
        bno086ok ? "true" : "false",
        bmp390ok ? "true" : "false");

    server.send(200, "application/json", json);
}

void handleGetSettings() {
    char json[448];
    snprintf(json, sizeof(json),
        "{\"mode\":%u,"
        "\"apSSID\":\"%s\",\"apPassword\":\"%s\","
        "\"stratuxSSID\":\"%s\","
        "\"sendAHRS\":%s,\"sendOwnship\":%s,"
        "\"sendHeartbeat\":%s,\"sendFFID\":%s,"
        "\"sendXGPS\":%s,\"sendXATT\":%s,"
        "\"invertRoll\":%s,"
        "\"enableBNO086\":%s,\"enableBMP390\":%s,\"enableGPS\":%s}",
        config.mode,
        config.apSSID, config.apPassword,
        config.stratuxSSID,
        config.sendAHRS      ? "true" : "false",
        config.sendOwnship   ? "true" : "false",
        config.sendHeartbeat ? "true" : "false",
        config.sendFFID       ? "true" : "false",
        config.sendXGPS       ? "true" : "false",
        config.sendXATT       ? "true" : "false",
        config.invertRoll     ? "true" : "false",
        config.enableBNO086   ? "true" : "false",
        config.enableBMP390   ? "true" : "false",
        config.enableGPS      ? "true" : "false");

    server.send(200, "application/json", json);
}

void handlePostSettings() {
    String body = server.arg("plain");

    // Minimal JSON parsing — look for key:value pairs
    auto jsonBool = [&](const char *key) -> int {
        int idx = body.indexOf(key);
        if (idx < 0) return -1;
        int colon = body.indexOf(':', idx);
        if (colon < 0) return -1;
        String val = body.substring(colon + 1, colon + 10);
        val.trim();
        return val.startsWith("true") ? 1 : 0;
    };

    auto jsonStr = [&](const char *key, char *dst, size_t maxLen) {
        String needle = String("\"") + key + "\"";
        int idx = body.indexOf(needle);
        if (idx < 0) return;
        int colon = body.indexOf(':', idx);
        if (colon < 0) return;
        int q1 = body.indexOf('"', colon + 1);
        if (q1 < 0) return;
        int q2 = body.indexOf('"', q1 + 1);
        if (q2 < 0) return;
        String val = body.substring(q1 + 1, q2);
        strlcpy(dst, val.c_str(), maxLen);
    };

    auto jsonInt = [&](const char *key) -> int {
        String needle = String("\"") + key + "\"";
        int idx = body.indexOf(needle);
        if (idx < 0) return -1;
        int colon = body.indexOf(':', idx);
        if (colon < 0) return -1;
        String val = body.substring(colon + 1, colon + 10);
        val.trim();
        return val.toInt();
    };

    int mode = jsonInt("mode");
    if (mode >= 0 && mode <= 2) config.mode = (uint8_t)mode;

    jsonStr("apSSID",      config.apSSID,      sizeof(config.apSSID));
    jsonStr("apPassword",  config.apPassword,  sizeof(config.apPassword));
    jsonStr("stratuxSSID", config.stratuxSSID,  sizeof(config.stratuxSSID));

    int v;
    v = jsonBool("sendAHRS");      if (v >= 0) config.sendAHRS      = v;
    v = jsonBool("sendOwnship");   if (v >= 0) config.sendOwnship   = v;
    v = jsonBool("sendHeartbeat"); if (v >= 0) config.sendHeartbeat  = v;
    v = jsonBool("sendFFID");      if (v >= 0) config.sendFFID       = v;
    v = jsonBool("sendXGPS");      if (v >= 0) config.sendXGPS       = v;
    v = jsonBool("sendXATT");      if (v >= 0) config.sendXATT       = v;
    v = jsonBool("invertRoll");    if (v >= 0) config.invertRoll     = v;
    v = jsonBool("enableBNO086");  if (v >= 0) config.enableBNO086   = v;
    v = jsonBool("enableBMP390");  if (v >= 0) config.enableBMP390   = v;
    v = jsonBool("enableGPS");     if (v >= 0) config.enableGPS      = v;

    saveConfig(config);

    server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot() {
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
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

    // ---- Load config from NVS (before sensor init so enable flags apply) ---
    loadConfig(config);
    Serial.print("Config loaded. Mode: ");
    Serial.println(config.mode == 0 ? "Auto" : config.mode == 1 ? "Force-Standalone" : "Force-Companion");

    // ---- BNO086 init -------------------------------------------------------
    if (config.enableBNO086) {
        Serial.print("Initializing BNO086... ");
        if (imu.begin(Wire)) {
            bno086ok = true;
            Serial.println("OK");
        } else {
            Serial.println("FAILED — BNO086 not found. Continuing without IMU.");
        }
    } else {
        Serial.println("BNO086 disabled in config.");
    }

    // ---- BMP390 init -------------------------------------------------------
    if (config.enableBMP390) {
        Serial.print("Initializing BMP390... ");
        if (baro.begin(Wire)) {
            bmp390ok = true;
            Serial.println("OK");
        } else {
            Serial.println("FAILED — BMP390 not found. Continuing without Baro.");
        }
    } else {
        Serial.println("BMP390 disabled in config.");
    }

    // ---- GPS init ----------------------------------------------------------
    if (config.enableGPS) {
        Serial.print("Initializing GPS (UART2)... ");
        gps.begin();
        Serial.println("OK (waiting for fix)");
    } else {
        Serial.println("GPS disabled in config.");
    }

    // ---- WiFi mode detection (respects config.mode) ------------------------
    Serial.println();
    if (config.mode == 0) {
        // Auto: scan for Stratux (current v0.2 behavior)
        if (scanForStratux()) {
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
    } else if (config.mode == 2) {
        // Force-Companion: skip scan, connect directly
        Serial.println("Force-Companion mode: connecting to Stratux...");
        stratuxSSID = config.stratuxSSID;
        if (connectToStratux()) {
            opMode = MODE_COMPANION;
            Serial.println();
            Serial.println(">>> COMPANION MODE (forced) <<<");
        } else {
            Serial.println("Stratux connection failed, falling back to Standalone.");
            opMode = MODE_STANDALONE;
        }
    }
    // config.mode == 1 (Force-Standalone) skips all scanning

    if (opMode == MODE_STANDALONE) {
        // ---- STANDALONE MODE -----------------------------------------------
        Serial.println();
        Serial.println(">>> STANDALONE MODE <<<");
        Serial.print("Starting WiFi AP... ");
        WiFi.mode(WIFI_AP);
        delay(100);
        WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
        delay(100);
        WiFi.softAP(config.apSSID, config.apPassword, 1, 0, 4);
        delay(500);
        Serial.print("SSID: ");
        Serial.print(config.apSSID);
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

    // ---- WebServer ---------------------------------------------------------
    server.on("/",             HTTP_GET,  handleRoot);
    server.on("/api/status",   HTTP_GET,  handleGetStatus);
    server.on("/api/settings", HTTP_GET,  handleGetSettings);
    server.on("/api/settings", HTTP_POST, handlePostSettings);
    server.on("/api/reboot",   HTTP_POST, handleReboot);
    server.begin();
    Serial.println("WebServer started on port 80");

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

    // ---- Handle HTTP requests (non-blocking, ~1ms) -------------------------
    server.handleClient();

    // ---- Companion mode: check WiFi connection, reconnect if lost ----------
    if (opMode == MODE_COMPANION && WiFi.status() != WL_CONNECTED) {
        if (now - lastWiFiReconnectMs >= WIFI_RECONNECT_MS) {
            lastWiFiReconnectMs = now;
            Serial.println("WARNING: Stratux WiFi lost, reconnecting...");
            connectToStratux();
        }
        // Don't send data while disconnected
        if (bno086ok) imu.update();
        if (config.enableGPS) gps.update();
        return;
    }

    // ---- 1. Read IMU (non-blocking) ----------------------------------------
    if (bno086ok) imu.update();

    // ---- Watchdog: re-init BNO086 if stale ---------------------------------
    if (bno086ok) {
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
    }

    // ---- 2. Read GPS NMEA (non-blocking) -----------------------------------
    if (config.enableGPS) gps.update();

    // ---- 3. Determine heading ----------------------------------------------
    float heading;
    if (config.enableGPS && gps.isTrackValid()) {
        heading = gps.getTrackDeg();
    } else if (bno086ok) {
        heading = imu.getYaw();
    } else {
        heading = 0;
    }

    // ---- 4. 5 Hz: ForeFlight AHRS + XATT ----------------------------------
    if (now - lastAHRSMs >= AHRS_INTERVAL_MS) {
        lastAHRSMs = now;

        float roll  = bno086ok ? (config.invertRoll ? -imu.getRoll() : imu.getRoll()) : 0;
        float pitch = bno086ok ? imu.getPitch() : 0;

        // ForeFlight binary AHRS (0x65 sub 0x01) on port 4000
        if (config.sendAHRS) {
            uint16_t ahrsLen = gdl90.buildForeFlightAHRS(frameBuf, roll, pitch, heading);
            broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ahrsLen);
        }

        // ForeFlight text XATT on port 49002
        if (config.sendXATT) {
            for (int t = 0; t < numTargets; t++) {
                gdl90.sendXATT(udpFFSim, targets[t], FF_SIM_PORT,
                               heading, pitch, roll);
            }
        }
    }

    // ---- 5. 1 Hz: GDL90 messages -------------------------------------------
    if (now - lastGDL90Ms >= GDL90_INTERVAL_MS) {
        lastGDL90Ms = now;

        // Read barometer
        if (bmp390ok) baro.update();

        // NIC/NACp
        bool gpsFix = config.enableGPS && gps.isFixValid();
        uint8_t nic  = gpsFix ? 8 : 0;
        uint8_t nacp = gpsFix ? 8 : 0;

        // Pressure altitude from BMP390
        int32_t pressAltFt = bmp390ok ? (int32_t)baro.getPressureAltFt() : 0;

        // GPS-derived values
        double lat     = config.enableGPS ? gps.getLatitude()   : 0;
        double lon     = config.enableGPS ? gps.getLongitude()  : 0;
        float  speedKt = config.enableGPS ? gps.getSpeedKnots() : 0;
        float  track   = (config.enableGPS && gps.isTrackValid()) ? gps.getTrackDeg() : heading;
        int32_t geoAlt = config.enableGPS ? (int32_t)gps.getAltitudeFt() : 0;

        // Heartbeat (0x00)
        if (config.sendHeartbeat) {
            uint16_t hbLen = gdl90.buildHeartbeat(frameBuf);
            broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, hbLen);
        }

        // ForeFlight Device ID (0x65 sub 0x00)
        if (config.sendFFID) {
            uint16_t ffLen = gdl90.buildForeFlightID(frameBuf);
            broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, ffLen);
        }

        // Ownship Report (0x0A) + Geometric Altitude (0x0B)
        if (config.sendOwnship) {
            uint16_t osLen = gdl90.buildOwnshipReport(frameBuf,
                                            lat, lon,
                                            pressAltFt,
                                            track, speedKt,
                                            nic, nacp);
            broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, osLen);

            uint16_t gaLen = gdl90.buildOwnshipGeoAlt(frameBuf, geoAlt);
            broadcastGDL90(udpGDL90, GDL90_PORT, frameBuf, gaLen);
        }

        // XGPS on port 49002
        if (config.sendXGPS) {
            float altMslM = (config.enableGPS ? gps.getAltitudeFt() : 0) * 0.3048f;
            float gsMs    = speedKt * 0.514444f;
            for (int t = 0; t < numTargets; t++) {
                gdl90.sendXGPS(udpFFSim, targets[t], FF_SIM_PORT,
                               lat, lon, altMslM, track, gsMs);
            }
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

        if (bno086ok) {
            float dbgRoll = config.invertRoll ? -imu.getRoll() : imu.getRoll();
            Serial.print("Roll: ");
            Serial.print(dbgRoll, 1);
            Serial.print("  Pitch: ");
            Serial.print(imu.getPitch(), 1);
            Serial.print("  Hdg: ");
            if (config.enableGPS && gps.isTrackValid()) {
                Serial.print(gps.getTrackDeg(), 1);
                Serial.print(" (GPS)");
            } else {
                Serial.print(imu.getYaw(), 1);
                Serial.print(" (IMU)");
            }
        } else {
            Serial.print("IMU: OFF");
        }

        if (bmp390ok) {
            Serial.print("  PressAlt: ");
            Serial.print(baro.getPressureAltFt(), 0);
            Serial.print(" ft");
        } else {
            Serial.print("  Baro: OFF");
        }

        if (config.enableGPS) {
            Serial.print("  GPS: ");
            if (gps.isFixValid()) {
                Serial.print("FIX");
            } else {
                Serial.print("NO FIX");
            }
            Serial.print("  Sats: ");
            Serial.print(gps.getSatellites());
        } else {
            Serial.print("  GPS: OFF");
        }

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
