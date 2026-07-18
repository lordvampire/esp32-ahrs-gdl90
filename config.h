#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// ---- Configuration struct --------------------------------------------------
struct Config {
    // Mode: 0=Auto, 1=Force-Standalone, 2=Force-Companion
    uint8_t mode;

    // WiFi AP settings (Standalone)
    char apSSID[32];
    char apPassword[32];

    // Stratux SSID to scan for (Companion)
    char stratuxSSID[32];

    // Message toggles
    bool sendAHRS;       // ForeFlight AHRS 0x65/0x01
    bool sendOwnship;   // Ownship Report 0x0A + Geo Alt 0x0B
    bool sendHeartbeat;  // Heartbeat 0x00
    bool sendFFID;       // ForeFlight ID 0x65/0x00
    bool sendXGPS;       // XGPS on port 49002
    bool sendXATT;       // XATT on port 49002

    // Sensor options
    bool invertRoll;
};

// ---- NVS namespace --------------------------------------------------------
static const char *NVS_NAMESPACE = "ahrs";

// ---- Default configuration -------------------------------------------------
inline void defaultConfig(Config &cfg) {
    cfg.mode = 0; // Auto
    strlcpy(cfg.apSSID,      "AHRS-GDL90", sizeof(cfg.apSSID));
    strlcpy(cfg.apPassword,  "ahrs1234",   sizeof(cfg.apPassword));
    strlcpy(cfg.stratuxSSID, "stratux",    sizeof(cfg.stratuxSSID));

    cfg.sendAHRS      = true;
    cfg.sendOwnship   = true;
    cfg.sendHeartbeat  = true;
    cfg.sendFFID       = true;
    cfg.sendXGPS       = true;
    cfg.sendXATT       = true;

    cfg.invertRoll     = true; // Current behavior (was hardcoded)
}

// ---- Load from NVS (fills defaults first, then overwrites with stored) -----
inline void loadConfig(Config &cfg) {
    defaultConfig(cfg);

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) { // read-only
        return; // NVS not available, keep defaults
    }

    cfg.mode = prefs.getUChar("mode", cfg.mode);

    if (prefs.isKey("apSSID")) {
        prefs.getString("apSSID", cfg.apSSID, sizeof(cfg.apSSID));
    }
    if (prefs.isKey("apPass")) {
        prefs.getString("apPass", cfg.apPassword, sizeof(cfg.apPassword));
    }
    if (prefs.isKey("stratuxSSID")) {
        prefs.getString("stratuxSSID", cfg.stratuxSSID, sizeof(cfg.stratuxSSID));
    }

    cfg.sendAHRS      = prefs.getBool("sendAHRS",      cfg.sendAHRS);
    cfg.sendOwnship   = prefs.getBool("sendOwnship",   cfg.sendOwnship);
    cfg.sendHeartbeat  = prefs.getBool("sendHeartbeat",  cfg.sendHeartbeat);
    cfg.sendFFID       = prefs.getBool("sendFFID",       cfg.sendFFID);
    cfg.sendXGPS       = prefs.getBool("sendXGPS",       cfg.sendXGPS);
    cfg.sendXATT       = prefs.getBool("sendXATT",       cfg.sendXATT);
    cfg.invertRoll     = prefs.getBool("invertRoll",     cfg.invertRoll);

    prefs.end();
}

// ---- Save to NVS -----------------------------------------------------------
inline void saveConfig(const Config &cfg) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) { // read-write
        return;
    }

    prefs.putUChar("mode",         cfg.mode);
    prefs.putString("apSSID",      cfg.apSSID);
    prefs.putString("apPass",      cfg.apPassword);
    prefs.putString("stratuxSSID", cfg.stratuxSSID);

    prefs.putBool("sendAHRS",      cfg.sendAHRS);
    prefs.putBool("sendOwnship",   cfg.sendOwnship);
    prefs.putBool("sendHeartbeat",  cfg.sendHeartbeat);
    prefs.putBool("sendFFID",       cfg.sendFFID);
    prefs.putBool("sendXGPS",       cfg.sendXGPS);
    prefs.putBool("sendXATT",       cfg.sendXATT);
    prefs.putBool("invertRoll",     cfg.invertRoll);

    prefs.end();
}

#endif
