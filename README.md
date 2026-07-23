# ESP32 AHRS GDL90

Open-source Aircraft Attitude and Heading Reference System (AHRS) for ESP32-S3.
Broadcasts GDL90 data over WiFi to **ForeFlight**, **Enroute** and other
EFB apps that support the GDL90 protocol.

Supports two operating modes:

- **Standalone Mode** -- creates its own WiFi AP. Connect your iPad/iPhone
  directly and get AHRS, GPS and barometric altitude in ForeFlight.
- **Companion Mode** -- automatically joins a nearby **Stratux** WiFi network
  and provides superior AHRS + barometric altitude alongside Stratux's ADS-B
  traffic reception.

All settings are configurable via a built-in **Web UI** -- no reflashing
required to change operating mode, message selection, sensor options or
WiFi credentials.

## Features

- **ForeFlight compatible** -- recognised as a connected device on the
  ForeFlight Devices page (uses the ForeFlight GDL90 Extended Specification)
- **Dual-mode operation** -- auto-detects Stratux WiFi at boot; if found,
  joins as a companion device; otherwise creates its own AP
- **Web Configuration UI** -- browser-based settings page with live sensor
  status, BNO086 calibration interface, and per-message/per-sensor toggles
- **Dual-port output** -- GDL90 binary on UDP 4000 + ForeFlight text protocol
  (XGPS/XATT) on UDP 49002
- **AHRS at 5 Hz** -- ForeFlight binary AHRS (0x65/0x01) and XATT attitude
  updates every 200 ms for smooth synthetic vision
- **Standard GDL90 at 1 Hz** -- Heartbeat, Ownship Report, Ownship Geometric
  Altitude and ForeFlight Device ID
- **BNO086 hardware sensor fusion** -- ARVR Stabilized Game Rotation Vector
  at 50 Hz for accurate roll/pitch without magnetometer interference;
  optional standard Rotation Vector mode with magnetic heading
- **BNO086 calibration with DCD persistence** -- guided calibration procedure
  via Web UI with live accuracy feedback; calibration data saved to BNO086
  flash and restored across power cycles
- **BMP390 barometric altitude** -- 4x more accurate than Stratux's BMP280
  (±0.03 hPa vs ±0.12 hPa relative accuracy)
- **GPS position & track** -- u-blox NMEA parser with automatic heading source
  switching (GPS track above 5 kt, IMU yaw below)
- **Per-sensor enable/disable** -- each sensor (BNO086, BMP390, GPS) can be
  individually enabled or disabled via Web UI; failed sensors don't block boot
- **Per-message toggles** -- each GDL90 message type can be enabled/disabled
  independently (useful for Companion Mode conflict avoidance)
- **Persistent configuration** -- all settings stored in ESP32 NVS flash;
  settings are saved separately from the running config and applied on reboot
- **Watchdog recovery** -- automatic BNO086 re-initialisation on sensor timeout;
  disables IMU gracefully if recovery fails
- **Auto-reconnect** -- in Companion Mode, automatically reconnects if Stratux
  WiFi is temporarily lost
- **Up to 4 simultaneous clients** (Standalone Mode)
- **3D-printable enclosure** -- parametric OpenSCAD case with mounting for
  ESP32 + BNO086 + BMP390 stack (STL files included)

## Hardware

| Component | Part | Interface | Address / Pins |
|-----------|------|-----------|----------------|
| MCU | SparkFun Thing Plus ESP32-S3 (WRL-24408) | -- | -- |
| IMU | BNO086 (SparkFun Qwiic) | I2C | 0x4B, SDA=GPIO8, SCL=GPIO9 |
| Barometer | BMP390 (Adafruit STEMMA QT) | I2C | 0x76 (SDO low), daisy-chained |
| GPS | u-blox module (SparkFun breakout) | UART2 | RX=GPIO16, TX=GPIO17, 9600 baud |

The BNO086 and BMP390 are connected via a Qwiic / STEMMA QT daisy chain on
the I2C bus. The SparkFun Thing Plus ESP32-S3 has a built-in Qwiic connector.

## Wiring

```
ESP32-S3 GPIO8  (SDA) ──── Qwiic SDA ──── BNO086 SDA ──── BMP390 SDA
ESP32-S3 GPIO9  (SCL) ──── Qwiic SCL ──── BNO086 SCL ──── BMP390 SCL
ESP32-S3 3V3           ──── Qwiic 3V3 ──── BNO086 3V3 ──── BMP390 3V3
ESP32-S3 GND           ──── Qwiic GND ──── BNO086 GND ──── BMP390 GND

ESP32-S3 GPIO16 (RX2) ──── GPS TX
ESP32-S3 GPIO17 (TX2) ──── GPS RX
ESP32-S3 VIN (5V)     ──── GPS VCC
ESP32-S3 GND          ──── GPS GND
```

The BNO086 and BMP390 are powered from the ESP32-S3 3.3 V rail via the Qwiic
connector. The GPS module is powered from the 5 V VIN pin (most u-blox
breakouts have an onboard regulator).

## Dependencies

Install the following libraries via **Sketch > Include Library > Manage
Libraries** in the Arduino IDE:

1. **SparkFun BNO08x Cortex Based IMU** by SparkFun Electronics
2. **Adafruit BMP3XX Library** by Adafruit (also installs Adafruit Unified Sensor)
3. **TinyGPSPlus** by Mikal Hart

### ESP32 Board Support

Add the following URL under **File > Preferences > Additional Board Manager
URLs**:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then install **esp32** by Espressif Systems (v3.x) in the Board Manager.

## Build & Flash

The SparkFun Thing Plus ESP32-S3 uses native USB CDC (no external USB-serial
chip). With Arduino IDE 1.8.x and esptool v5.x, a two-step process is
required:

```bash
# Step 1: Compile with explicit build path
flatpak run cc.arduino.arduinoide --verify \
  --board esp32:esp32:sparkfun_esp32s3_thing_plus:UploadSpeed=921600,CDCOnBoot=cdc \
  --pref build.path=build \
  esp32-ahrs-gdl90.ino

# Step 2: Flash merged binary
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  write-flash 0x0 build/esp32-ahrs-gdl90.ino.merged.bin
```

To reset settings to factory defaults, erase the NVS partition before flashing:

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  erase-region 0x9000 0x5000
```

### Arduino IDE GUI

| Setting | Value |
|---------|-------|
| Board | SparkFun Thing Plus ESP32-S3 |
| USB CDC On Boot | Enabled |
| Upload Speed | 921600 |
| Port | `/dev/ttyACM0` (Linux) or the appropriate COM port |

## Web Configuration UI

Once running, open `http://192.168.10.1` in a browser (Standalone Mode)
or the device's assigned IP (Companion Mode).

### Live Status

Real-time display updated every second:
- Roll, Pitch, Heading (with source: GPS or IMU)
- Barometric altitude
- GPS fix status and satellite count
- WiFi clients / RSSI
- Sensor status indicators (green = OK, red = failed)
- Uptime

### BNO086 Calibration

Guided three-step calibration with live accuracy feedback:

1. **Gyro** -- place device flat, hold still for 3 seconds
2. **Accel** -- rotate to 4-6 unique orientations, hold each 1 second
3. **Mag** -- rotate ~180° and back in each axis (roll, pitch, yaw)

Accuracy indicators (0-3) are color-coded: red (unreliable), orange (low),
yellow (medium), green (high). When all sensors reach 2+, click **Save** to
persist calibration data (DCD) to the BNO086's internal flash.

### Settings

All settings are persisted in NVS flash and applied on next reboot.

**Operating Mode:**
- Auto -- scans for Stratux WiFi, falls back to Standalone
- Force Standalone -- always creates own AP
- Force Companion -- always tries to join Stratux

**WiFi:**
- AP SSID and password (Standalone Mode)
- Stratux SSID to scan for (Companion Mode)

**Message Toggles:**
- AHRS (0x65/0x01)
- Ownship Report (0x0A + 0x0B)
- Heartbeat (0x00)
- ForeFlight ID (0x65/0x00)
- XGPS (port 49002)
- XATT (port 49002)

**Sensor Options:**
- BNO086 (IMU) enable/disable
- BMP390 (Baro) enable/disable
- GPS enable/disable
- Invert Roll axis
- Game Rotation Vector (no magnetometer; default on)

### REST API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Web UI (HTML page) |
| GET | `/api/status` | Live sensor data, accuracy, stability, calibration state |
| GET | `/api/settings` | Current saved settings (from NVS) |
| POST | `/api/settings` | Save settings to NVS (applied on reboot) |
| POST | `/api/reboot` | Reboot the device |
| POST | `/api/calibration/start` | Enter calibration mode (accel+gyro+mag) |
| POST | `/api/calibration/save` | Save DCD to BNO086 flash, return to flight mode |
| POST | `/api/calibration/cancel` | Cancel calibration, return to flight mode |

## Operating Modes

### Standalone Mode

When no Stratux WiFi is found the device creates its own access point:

| Parameter | Default |
|-----------|---------|
| SSID | `AHRS-GDL90` (configurable) |
| Password | `ahrs1234` (configurable) |
| IP Address | 192.168.10.1 |
| GDL90 Port | UDP 4000 |
| ForeFlight Text Port | UDP 49002 |

The device provides all data: AHRS, GPS, barometric altitude, Heartbeat and
ForeFlight Device ID.

### Companion Mode (with Stratux)

At boot the device scans for WiFi networks matching the configured Stratux
SSID (default: `stratux`, case-insensitive). If found, it joins the Stratux
network as a client and broadcasts on the Stratux subnet.

```
┌──────────┐                              ┌──────────┐
│  Stratux │  ADS-B Traffic (0x14)        │  ESP32   │
│          │──────────────────────────►    │  AHRS    │
│  ADS-B   │                         │    │          │
│  receiver│                         │    │  BNO086  │──► AHRS      (0x65/0x01) @ 5 Hz
│          │                         │    │  BMP390  │──► Ownship   (0x0A)      @ 1 Hz
└──────────┘                         │    │  GPS     │──► GeoAlt    (0x0B)      @ 1 Hz
                   ┌─────────┐       │    │          │──► Heartbeat (0x00)      @ 1 Hz
                   │  iPad   │◄──────┘    │          │──► FF-ID     (0x65/0x00) @ 1 Hz
                   │ForeFlight◄───────────┘          │──► XGPS/XATT            @ 1-5 Hz
                   └─────────┘                       └──────────┘
```

**Recommended Stratux settings** (to avoid conflicts):
- AHRS: **off** (ESP32 provides better AHRS via BNO086)
- Ownship Report: **off** (ESP32 provides it with BMP390 baro)
- ADS-B traffic: **on**

Alternatively, use the Web UI to disable Ownship and ForeFlight ID on the
ESP32 side -- whichever approach avoids duplicate messages on the subnet.

## Sensor Fusion Modes

| Aspect | ARVR Stab. Rotation Vector | ARVR Stab. Game Rotation Vector (default) |
|--------|----------------------------|-------------------------------------------|
| Heading source | Magnetometer | Gyro-only (drifts slowly) |
| Cockpit interference | Affected by avionics/engine magnetics | Immune |
| Pitch/Roll accuracy | 1.5-2.5° | 1.0-2.5° |
| Calibration needed | Accel + Gyro + Mag | Accel + Gyro only |
| Best for | Handheld / unshielded use | Cockpit-mounted (recommended) |

The Game Rotation Vector is the default because cockpit magnetic interference
makes magnetometer-based heading unreliable. GPS track is used as the primary
heading source above 5 kt ground speed.

## Sensor Comparison

| Sensor | Stratux (stock) | ESP32 AHRS (this project) |
|--------|-----------------|---------------------------|
| IMU | ICM-20948 (software Kalman) | BNO086 (hardware fusion, 50 Hz) |
| Barometer | BMP280 (±0.12 hPa) | BMP390 (±0.03 hPa) |
| Baro noise | 1.3 Pa RMS | 0.02 Pa RMS (65x less) |

## EFB App Setup

### ForeFlight -- Standalone Mode

1. Connect your iPad to the **AHRS-GDL90** WiFi network (password: `ahrs1234`).
2. Go to **More > Devices**.
3. The device appears as **"AHRS-GDL"** with a connected status.
4. AHRS attitude data is shown on the synthetic vision / attitude indicator.

### ForeFlight -- Companion Mode (with Stratux)

1. Power on Stratux and the ESP32 AHRS.
2. The ESP32 automatically joins the Stratux WiFi (serial monitor shows
   `>>> COMPANION MODE <<<`).
3. Connect your iPad to the **Stratux** WiFi network.
4. In ForeFlight go to **More > Devices** -- you should see the AHRS device
   and Stratux traffic.
5. In Stratux settings, disable AHRS and Ownship to avoid conflicts.

### Enroute Flight Navigation

1. Connect to the **AHRS-GDL90** WiFi network (Standalone) or **Stratux**
   network (Companion Mode).
2. Enroute automatically detects GDL90 data on port 4000.

### Other GDL90-compatible EFBs

Any EFB that supports the GDL90 protocol on UDP port 4000 should work
(e.g. SkyDemon, AvPlan EFB, FlyQ).

## Protocol Details

### Port 4000 -- GDL90 Binary

| Message | ID | Rate | Content |
|---------|----|------|---------|
| Heartbeat | `0x00` | 1 Hz | System status, UTC timestamp |
| Ownship Report | `0x0A` | 1 Hz | Lat/lon, pressure altitude, track, ground speed, NIC/NACp |
| Ownship Geo Alt | `0x0B` | 1 Hz | GPS geometric altitude |
| ForeFlight ID | `0x65` sub `0x00` | 1 Hz | Device serial, short name, long name, capabilities |
| ForeFlight AHRS | `0x65` sub `0x01` | 5 Hz | Roll, pitch, heading (1/10 deg, binary) |

All messages use GDL90 framing (0x7E flag bytes, byte-stuffing, CRC-16 CCITT).

### Port 49002 -- ForeFlight Text Protocol

| Message | Rate | Format |
|---------|------|--------|
| XGPS | 1 Hz | `XGPSAHRS-GDL,<lon>,<lat>,<alt_m>,<track>,<gs_m/s>` |
| XATT | 5 Hz | `XATTAHRS-GDL,<heading>,<pitch>,<roll>` |

## Heading Logic

- **GPS track** is used as heading when the fix is valid and ground speed
  exceeds 5 knots.
- **BNO086 yaw** is used as heading when there is no GPS fix or speed is
  below 5 knots.

## Altitude

- **Pressure altitude** (BMP390) -- used in the Ownship Report. Computed
  using the ISA standard atmosphere formula with QNH = 1013.25 hPa.
- **Geometric altitude** (GPS) -- used in the Ownship Geometric Altitude
  message.

## Serial Debug Output

At 115200 baud the system prints status every 2 seconds:

```
[STANDALONE] Roll: -2.3  Pitch: 5.1  Hdg: 270.4 (GPS)  PressAlt: 3250 ft  GPS: FIX  Sats: 9  Clients: 1
[COMPANION]  Roll: -2.3  Pitch: 5.1  Hdg: 270.4 (GPS)  PressAlt: 3250 ft  GPS: FIX  Sats: 9  WiFi: OK  RSSI: -45
```

## Error Handling

- If a sensor (BNO086, BMP390, GPS) is not detected at startup it is disabled
  and the system continues without it. The Web UI shows the sensor status.
- If the BNO086 stops reporting for more than 2 seconds a re-initialisation
  is attempted automatically (max once per 10 s). If re-init fails, the IMU
  is disabled gracefully.
- If GPS has no fix, NIC and NACp are set to 0, heading falls back to the
  IMU, and only pressure altitude is available.

## 3D Printed Case

A parametric OpenSCAD enclosure is included in the `case/` directory.
Pre-exported STL files are ready for slicing.

```
case/
  esp32_case_v2.scad         Parametric source (OpenSCAD)
  esp32_case_v2_base.stl     Base shell with screw bosses, USB-C and cable slots
  esp32_case_v2_lid.stl      Snap-fit lid with BNO086 + BMP390 mounting
```

**Dimensions:** 62 x 31 x 16 mm. Fits the SparkFun Thing Plus ESP32-S3 in the
base with BNO086 and BMP390 mounted on the lid via M2 standoffs.

To re-export STLs after editing:

```bash
openscad -D 'part="base"' -o case/esp32_case_v2_base.stl case/esp32_case_v2.scad
openscad -D 'part="lid"'  -o case/esp32_case_v2_lid.stl  case/esp32_case_v2.scad
```

## Project Structure

```
esp32-ahrs-gdl90/
  esp32-ahrs-gdl90.ino   Main sketch -- WiFi, timing loops, sensor polling, web server
  gdl90.h / gdl90.cpp    GDL90 protocol -- CRC-16, framing, message builders
  bno086.h / bno086.cpp   BNO086 IMU -- init, quaternion to Euler, calibration, watchdog
  bmp390.h / bmp390.cpp   BMP390 barometer -- pressure to altitude conversion
  gps.h / gps.cpp         u-blox GPS NMEA parser (TinyGPSPlus wrapper)
  config.h                Configuration struct, NVS load/save, defaults
  webui.h                 Embedded HTML/CSS/JS for the Web Configuration UI
  case/                   3D-printable enclosure (OpenSCAD + STL)
```

## License

MIT License. See [LICENSE](LICENSE) for details.
