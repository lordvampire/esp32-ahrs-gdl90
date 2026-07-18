# ESP32 AHRS GDL90 — Project Notes

## Current State (v0.2 + roll fix)

Working ForeFlight-compatible AHRS with dual-mode operation:
- **Standalone Mode**: Own WiFi AP, full GDL90 output
- **Companion Mode**: Auto-joins Stratux WiFi, broadcasts on Stratux subnet

### Known Issues (Companion Mode)
1. **Ownship conflict**: When Stratux GPS is enabled, both devices send
   Ownship Report (0x0A) → ForeFlight AHRS becomes erratic
2. **Baro not reaching ForeFlight**: BMP390 data in our Ownship Report gets
   overridden by Stratux's Ownship Report (BMP280 data)
3. **Device name flicker**: Both devices send ForeFlight ID (0x65/0x00),
   name alternates in ForeFlight Devices page
4. **Workaround**: Disable GPS, AHRS, and Baro in Stratux settings — then
   everything works correctly

### Hardware
- ESP32-WROOM-32D (not NodeMCU — board type: `esp32:esp32:esp32`)
- BNO086 I2C 0x4B (SparkFun Qwiic)
- BMP390 I2C 0x77
- u-blox GPS UART2 (GPIO16/17, 9600 baud)

### Build Command
```bash
flatpak run cc.arduino.arduinoide --upload \
  --board esp32:esp32:esp32:UploadSpeed=921600 \
  --port /dev/ttyUSB0 \
  esp32-ahrs-gdl90.ino
```

---

## Project v0.3 — Web Configuration Interface

**Goal**: Browser-based settings page on the ESP32 (like Stratux has), reachable
via `http://192.168.10.1` (Standalone) or assigned IP (Companion).

**Approach**: ESP32 WebServer (Ansatz A) — simple HTTP server with inline
HTML/CSS/JS, settings stored in NVS (ESP32 flash).

### Configurable Parameters
- **Mode**: Standalone / Companion / Force-Standalone (skip Stratux scan)
- **AHRS senden**: on/off
- **Ownship Report senden**: on/off (fixes Companion Mode conflict)
- **ForeFlight ID senden**: on/off (fixes name flicker)
- **WiFi SSID/Password**: for own AP
- **Stratux SSID**: custom name (default: "stratux")
- **Roll inversion**: on/off (for different BNO086 mounting orientations)
- **Sensor status**: Live display of Roll/Pitch/Heading/Baro/GPS

### Implementation Plan
1. Add `<WebServer.h>` and `<Preferences.h>` (NVS) to the sketch
2. Create HTML page as `const char[]` in a new `webui.h`
3. REST endpoints:
   - `GET /` — main settings page
   - `GET /api/status` — JSON with live sensor data + mode
   - `POST /api/settings` — save settings to NVS
   - `GET /api/settings` — read current settings
4. Load settings from NVS on boot, apply before WiFi init
5. WebServer runs on port 80 in both modes

### Resolves Known Issues
- Disabling Ownship Report in Companion Mode → no more GPS conflict
- Disabling ForeFlight ID in Companion Mode → no more name flicker
- Baro will work when Ownship is sole source (Stratux Ownship disabled)

---

## Project v0.4 — Stratux BNO086 Integration (Go Driver)

**Goal**: Integrate BNO086 directly into Stratux as a native AHRS sensor,
replacing the ICM-20948 software Kalman filter with BNO086 hardware fusion.

### IMPORTANT: Existing Work Already Done!

A complete Go BNO086 I2C-SHTP driver and Stratux integration already exists
in the `lordvampire/stratux` fork on GitHub:

**Repository**: https://github.com/lordvampire/stratux (branch: `master`)

#### Files already written:

| File | Description | Size |
|------|-------------|------|
| `sensors/bno086.go` | Full Go I2C-SHTP driver | ~430 lines |
| `main/sensors.go` | Parallel AHRS integration (alongside ICM20948) | Modified |
| `main/managementinterface.go` | HTTP endpoints for BNO086 | Modified |
| `web/plates/ahrs-bno085.html` | BNO086 AHRS WebUI display | New |
| `web/plates/ahrs-compare.html` | ICM20948 vs BNO086 comparison page | New |
| `web/plates/js/ahrs-bno085.js` | AngularJS controller for BNO086 display | New |
| `web/plates/js/ahrs-compare.js` | Comparison controller with delta calc | New |
| `BNO086_README.md` | German-language setup documentation | New |

#### Key commits:
- `51fbb28` — "Add BNO086 parallel AHRS system with I2C/Qwiic support"
- `2319341` — "Update README with BNO086 documentation"

#### Driver details (`sensors/bno086.go`):
- Pure Go I2C-SHTP (Sensor Hub Transport Protocol) implementation
- Auto-detects BNO086 at I2C address 0x4A or 0x4B
- Enables Game Rotation Vector (0x08), Gyroscope (0x02), Accelerometer (0x01)
- 100 Hz internal read loop
- Implements Stratux `IMUReader` interface (`Read()`, `ReadOne()`, `Close()`)
- Quaternion-to-Euler conversion
- Built-in mock mode for testing without hardware
- Runs as parallel AHRS system alongside existing ICM-20948

#### Integration approach used:
- **Parallel system** — BNO086 AHRS runs in separate goroutines alongside ICM-20948
- BNO086 data goes through Stratux's Kalman filter (using raw accel/gyro reports)
- Game Rotation Vector provides quaternion → Euler for comparison/validation
- WebUI comparison page shows deltas between ICM-20948 and BNO086 attitude

#### What was NOT completed / What blocked progress:
- The driver reads raw accel/gyro from BNO086 and feeds them to the Kalman filter,
  but the BNO086's main advantage (hardware fusion) is not fully utilized
- Unknown whether it was tested on actual Stratux hardware (Raspberry Pi)
- May need work to make it the PRIMARY AHRS instead of a parallel system
- The ideal approach: bypass Kalman filter entirely for BNO086, use its
  hardware-fused quaternion directly as the attitude output

### Potential v0.4 plan:
1. Review existing code in `lordvampire/stratux` fork
2. Test on Raspberry Pi with BNO086 connected via Qwiic/I2C
3. Option A: Use existing parallel approach (already coded)
4. Option B: Make BNO086 the primary AHRS, bypass Kalman filter
5. Add setting in Stratux UI to select AHRS source (ICM-20948 / BNO086)

### Also relevant:
- `lordvampire/goflying` fork — ICM-20948 magnetometer fixes (AK09916)
  - Branch `master`: magnetometer support
  - Branch `stratux_master`: USER_CTRL register fix
- Local: `/home/faruktuefekli/GitHub/icm20948/` — ICM-20948 docs + Stratux code
- Local: `/home/faruktuefekli/GitHub/icm20948/CLAUDE.md` — AHRS architecture guide
- Local: `/home/faruktuefekli/GitHub/icm20948/STRATUX_CHANGES.md` — Magnetometer integration

---

## Reference: Sensor Comparison

| Sensor | Stratux (stock) | ESP32 AHRS (this project) | Stratux + BNO086 (v0.4) |
|--------|-----------------|---------------------------|-------------------------|
| IMU | ICM-20948 (SW Kalman) | BNO086 (HW fusion) | BNO086 (HW fusion) |
| Baro | BMP280 (±0.12 hPa) | BMP390 (±0.03 hPa) | BMP280 or BMP390 |
| GPS | u-blox (on Pi) | u-blox (on ESP32) | u-blox (on Pi) |
| ADS-B | SDR receiver | none | SDR receiver |
| AHRS quality | Good (SW) | Better (HW) | Best (HW, native) |
