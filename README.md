# ESP32 AHRS GDL90

Open-source Aircraft Attitude and Heading Reference System (AHRS) for ESP32.
Broadcasts GDL90 data over WiFi to **ForeFlight**, **Enroute** and other
EFB apps that support the GDL90 protocol.

Supports two operating modes:

- **Standalone Mode** -- creates its own WiFi AP. Connect your iPad/iPhone
  directly and get AHRS, GPS and barometric altitude in ForeFlight.
- **Companion Mode** -- automatically joins a nearby **Stratux** WiFi network
  and provides superior AHRS + barometric altitude alongside Stratux's ADS-B
  traffic reception.

## Features

- **ForeFlight compatible** -- recognised as a connected device on the
  ForeFlight Devices page (uses the ForeFlight GDL90 Extended Specification)
- **Dual-mode operation** -- auto-detects Stratux WiFi at boot; if found,
  joins as a companion device; otherwise creates its own AP
- **Dual-port output** -- GDL90 binary on UDP 4000 + ForeFlight text protocol
  (XGPS/XATT) on UDP 49002
- **AHRS at 5 Hz** -- ForeFlight binary AHRS (0x65/0x01) and XATT attitude
  updates every 200 ms for smooth synthetic vision
- **Standard GDL90 at 1 Hz** -- Heartbeat, Ownship Report, Ownship Geometric
  Altitude and ForeFlight Device ID
- **BNO086 hardware sensor fusion** -- ARVR Stabilized Rotation Vector for
  accurate roll/pitch/heading without software filtering (significantly
  better than Stratux's software-based ICM-20948 AHRS)
- **BMP390 barometric altitude** -- 4x more accurate than Stratux's BMP280
  (±0.03 hPa vs ±0.12 hPa relative accuracy)
- **GPS position & track** -- u-blox NMEA parser with automatic heading source
  switching (GPS track above 5 kt, IMU yaw below)
- **Watchdog recovery** -- automatic BNO086 re-initialisation on sensor timeout
- **Auto-reconnect** -- in Companion Mode, automatically reconnects if Stratux
  WiFi is temporarily lost
- **Up to 4 simultaneous clients** (Standalone Mode)

## Hardware

| Component | Part | Interface | Address / Pins |
|-----------|------|-----------|----------------|
| MCU | ESP32-WROOM-32D (Xtensa LX6, 240 MHz) | -- | -- |
| IMU | BNO086 (SparkFun Qwiic) | I2C | 0x4A, SDA=GPIO21, SCL=GPIO22 |
| Barometer | BMP390 (Adafruit / SparkFun Qwiic) | I2C | 0x77, daisy-chained |
| GPS | u-blox module (SparkFun breakout) | UART2 | RX=GPIO16, TX=GPIO17, 9600 baud |

Any ESP32 dev board with a WROOM-32 module should work (DevKitC, NodeMCU-32S,
etc.). The BNO086 and BMP390 are connected via a Qwiic / I2C daisy chain.

## Wiring

```
ESP32 GPIO21 (SDA) ──── Qwiic SDA ──── BNO086 SDA ──── BMP390 SDA
ESP32 GPIO22 (SCL) ──── Qwiic SCL ──── BNO086 SCL ──── BMP390 SCL
ESP32 3V3          ──── Qwiic 3V3 ──── BNO086 3V3 ──── BMP390 3V3
ESP32 GND          ──── Qwiic GND ──── BNO086 GND ──── BMP390 GND

ESP32 GPIO16 (RX2) ──── GPS TX
ESP32 GPIO17 (TX2) ──── GPS RX
ESP32 VIN (5V)     ──── GPS VCC
ESP32 GND          ──── GPS GND
```

The BNO086 and BMP390 are powered from the ESP32 3.3 V rail. The GPS module
is powered from the 5 V VIN pin (most u-blox breakouts have an onboard
regulator).

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

Then install **esp32** by Espressif Systems in the Board Manager.

## Build & Flash

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| CPU Frequency | 240 MHz |
| Flash Size | 4 MB |
| Upload Speed | 921600 |
| Port | your USB serial port |

### Command line (Arduino IDE 1.8.x via Flatpak)

```bash
# Compile
flatpak run cc.arduino.arduinoide --verify \
  --board esp32:esp32:esp32:UploadSpeed=921600 \
  esp32-ahrs-gdl90.ino

# Flash
flatpak run cc.arduino.arduinoide --upload \
  --board esp32:esp32:esp32:UploadSpeed=921600 \
  --port /dev/ttyUSB0 \
  esp32-ahrs-gdl90.ino
```

## Operating Modes

### Standalone Mode

When no Stratux WiFi is found the device creates its own access point:

| Parameter | Value |
|-----------|-------|
| SSID | `AHRS-GDL90` |
| Password | `ahrs1234` |
| IP Address | 192.168.10.1 |
| GDL90 Port | UDP 4000 |
| ForeFlight Text Port | UDP 49002 |
| Broadcast | 192.168.10.255 |

The ESP32 provides all data: AHRS, GPS, barometric altitude, Heartbeat and
ForeFlight Device ID.

### Companion Mode (with Stratux)

At boot the device scans for WiFi networks named `stratux` or `Stratux`
(case-insensitive). If found, it joins the Stratux network as a client
and broadcasts on the Stratux subnet.

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

**Stratux settings** (recommended for Companion Mode):
- AHRS: **off** (ESP32 provides better AHRS via BNO086)
- Ownship Report: **off** (ESP32 provides it with BMP390 baro)
- ADS-B traffic: **on**

**Sensor comparison:**

| Sensor | Stratux | ESP32 AHRS | Advantage |
|--------|---------|------------|-----------|
| IMU | ICM-20948 (software Kalman) | BNO086 (hardware fusion) | Much lower drift & latency |
| Barometer | BMP280 (±0.12 hPa) | BMP390 (±0.03 hPa) | 4x more accurate |
| Baro noise | 1.3 Pa RMS | 0.02 Pa RMS | 65x less noise |

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
- **BNO086 yaw** (ARVR Stabilized Rotation Vector) is used as heading when
  there is no GPS fix or speed is below 5 knots.

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

- If BNO086 or BMP390 is not detected at startup the system halts with an
  error message on Serial.
- If the BNO086 stops reporting for more than 2 seconds a re-initialisation
  is attempted automatically (max once per 10 s).
- If GPS has no fix, NIC and NACp are set to 0, heading falls back to the
  IMU, and only pressure altitude is available.

## Project Structure

```
esp32-ahrs-gdl90/
  esp32-ahrs-gdl90.ino   Main sketch -- WiFi AP, timing loops, sensor polling
  gdl90.h / gdl90.cpp    GDL90 protocol -- CRC-16, framing, message builders
  bno086.h / bno086.cpp   BNO086 IMU driver -- quaternion to Euler conversion
  bmp390.h / bmp390.cpp   BMP390 barometer driver -- pressure altitude
  gps.h / gps.cpp         u-blox GPS NMEA parser
```

## License

MIT License. See [LICENSE](LICENSE) for details.
