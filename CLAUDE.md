# ESP32 AHRS GDL90 — Project Notes

## Current State (v0.5)

Working ForeFlight-compatible AHRS with dual-mode operation:
- **Standalone Mode**: Own WiFi AP, full GDL90 output
- **Companion Mode**: Auto-joins Stratux WiFi, broadcasts on Stratux subnet
- **Web Configuration**: Browser-based settings via `http://192.168.10.1`
- **Per-Sensor Config**: BNO086, BMP390 and GPS individually enable/disable via WebUI, persisted in NVS
- **BNO086 Calibration**: DCD persistence, accuracy monitoring, guided calibration UI
- **Sensor Fusion**: Configurable ARVR Stabilized Game RV (default) or standard RV, 50Hz report rate
- **ESP32-S3**: Migrated to SparkFun Thing Plus ESP32-S3 (I2C on GPIO 8/9)

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
- SparkFun Thing Plus ESP32-S3 (WRL-24408, board type: `esp32:esp32:esp32s3`)
- BNO086 I2C 0x4B (SparkFun Qwiic)
- BMP390 I2C 0x77
- u-blox GPS UART2 (GPIO16/17, 9600 baud)
- I2C SDA: GPIO 8, SCL: GPIO 9

### Build Command
```bash
flatpak run cc.arduino.arduinoide --upload \
  --board esp32:esp32:esp32s3:UploadSpeed=921600,CDCOnBoot=1 \
  --port /dev/ttyACM0 \
  esp32-ahrs-gdl90.ino
```

### REST Endpoints
- `GET /` — main settings page
- `GET /api/status` — JSON with live sensor data, accuracy, stability, calibration state
- `GET /api/settings` — read current settings
- `POST /api/settings` — save settings to NVS
- `POST /api/reboot` — reboot device
- `POST /api/calibration/start` — enter calibration mode (accel+gyro+mag)
- `POST /api/calibration/save` — save DCD to flash, return to flight mode
- `POST /api/calibration/cancel` — cancel calibration, return to flight mode

---

## v0.3 — Web Configuration Interface (completed)

Browser-based settings page on the ESP32, reachable via `http://192.168.10.1`
(Standalone) or assigned IP (Companion). ESP32 WebServer with inline HTML/CSS/JS,
settings stored in NVS.

### REST Endpoints
- `GET /` — main settings page
- `GET /api/status` — JSON with live sensor data + mode
- `POST /api/settings` — save settings to NVS
- `GET /api/settings` — read current settings

### Resolved Known Issues
- Disabling Ownship Report in Companion Mode → no more GPS conflict
- Disabling ForeFlight ID in Companion Mode → no more name flicker
- Baro will work when Ownship is sole source (Stratux Ownship disabled)

---

## v0.4 — Per-Sensor Enable/Disable Config (completed)

**Goal**: Each sensor (BNO086, BMP390, GPS) can be individually enabled/disabled
via WebUI. Disabled or failed sensors no longer block boot.

### Changes
- **Config flags**: `enableBNO086`, `enableBMP390`, `enableGPS` in `Config` struct,
  persisted in NVS
- **Graceful boot**: Sensor init failures log a warning and continue instead of
  halting with `while(true)`
- **Runtime guards**: All sensor reads, GDL90 fields, and debug output are
  guarded by `bno086ok` / `bmp390ok` / `config.enableGPS` flags
- **WebUI**: Three new toggles under "Sensor Options" section
- **Status endpoint**: New `gpsEnabled` field; `bno086ok` and `bmp390ok` reflect
  actual init success (not just watchdog state)
- **Config load moved before sensor init** so enable flags are available at boot

### Files Changed
- `config.h` — new fields, NVS load/save keys (`enBNO086`, `enBMP390`, `enGPS`)
- `esp32-ahrs-gdl90.ino` — conditional init, runtime guards, reordered setup()
- `webui.h` — new toggle controls, updated JS for load/save/status

---

## Case v2 — 3D Printed Enclosure (completed)

Parametric OpenSCAD case for ESP32 + BNO086 + BMP390 stack.

**File**: `case/esp32_case_v2.scad`

### Dimensions
- **Outer**: 62.22 × 30.66 × 15.6 mm (box_h = floor 1.6 + inner 14.0)
- **Cavity**: 59.02 × 27.46 × 14.0 mm (asymmetric width, +4mm connector side)
- **inner_h = 14mm**: Fits two stacked boards (ESP32 in base, BNO086+BMP390 on lid)

### Base
- Rounded-corner shell with 4 screw bosses (M2.5, Feather/Thing Plus hole pattern)
- USB-C closed opening (10 × 7 mm, not open to top)
- Qwiic + LiPo cable slots through connector-side wall
- Wave-shaped cooling vents through floor (shortened to clear bosses)

### Lid
- Snap-fit lip (2mm depth, 0.2mm clearance)
- **BNO086** (SparkFun SEN-22857, 30.48 × 25.4 mm): 2× M2 screw bosses, 24mm spacing along X, anti-rotation support on far Y edge
- **BMP390** (Adafruit 4816 STEMMA QT, 17.78 × 25.4 mm rotated): 2× M2 screw bosses, 20mm spacing along Y, anti-rotation support on far X edge
- 8mm between nearest BNO086 and BMP390 holes
- Both sensors at 1.5mm standoff height (clears backside SMD components)
- Part selector via `-D 'part="base|lid|assembled|both"'`

### STL Export
```bash
openscad -D 'part="base"' -o case/esp32_case_v2_base.stl case/esp32_case_v2.scad
openscad -D 'part="lid"'  -o case/esp32_case_v2_lid.stl  case/esp32_case_v2.scad
```

### Height Stack (assembled, from bottom)
```
Floor          0.0 – 1.6 mm
ESP32 bosses   1.6 – 3.6 mm  (ledge_h = 2.0)
ESP32 PCB      3.6 – 5.2 mm
ESP32 tallest  5.2 – 7.6 mm
  ↕ 1.3 mm clearance (Qwiic cable)
BNO086 Qwiic   8.9 mm
BNO086 PCB    10.9 – 12.5 mm
Standoffs     12.5 – 14.0 mm
Lid plate     14.0 – 15.6 mm
```

---

## v0.5 — BNO086 Calibration & Sensor Fusion + ESP32-S3 Migration (completed)

**Goal**: Proper BNO086 calibration with DCD persistence, optimized sensor fusion mode
for cockpit use, WebUI calibration interface, and migration to ESP32-S3.

### Changes

#### ESP32-S3 Migration
- I2C pins changed: SDA 21→8, SCL 22→9 (SparkFun Thing Plus ESP32-S3)
- Hardware comment updated
- GPS pins (GPIO 16/17) unchanged

#### BNO086 Calibration & Sensor Fusion
- **DCD persistence**: `saveDCD()` saves calibration data to BNO086 flash via `saveCalibration()`
- **Flight mode**: Default calibration config is `SH2_CAL_ACCEL` only (no mag dynamic cal during flight)
- **Calibration mode**: `startCalibration()` enables accel+gyro+mag dynamic cal + 2Hz mag report
- **Accuracy monitoring**: `getAccuracy()` (quaternion 0-3), `getMagAccuracy()` (mag field 0-3)
- **Stability classifier**: `getStability()` at 1Hz (0=unknown, 1=table, 2=stationary, 3=stable, 4=motion)
- **Game Rotation Vector**: Default sensor fusion mode switched to ARVR Stabilized GRV (no magnetometer, immune to cockpit magnetic interference). Configurable via `useGameRV` setting.
- **50Hz report rate**: `BNO086_REPORT_INTERVAL_MS` changed from 100ms (10Hz) to 20ms (50Hz)
- **Reset recovery**: `wasReset()` handler restores full calibration config, reports, and stability classifier

#### WebUI
- New "BNO086 Calibration" card with live accuracy display (color-coded: red/orange/yellow/green)
- Stability classifier display
- Guided calibration procedure (Gyro → Accel → Mag) with step completion checkmarks
- Calibrate/Save/Cancel buttons with dynamic visibility
- "Game Rotation Vector (no Mag)" checkbox in Sensor Options

#### API
- Status JSON extended: `imuAccuracy`, `magAccuracy`, `stability`, `calibrating`, `useGameRV`
- Settings JSON extended: `useGameRV`
- New endpoints: `/api/calibration/start`, `/api/calibration/save`, `/api/calibration/cancel`
- Status JSON buffer increased from 448 to 600 bytes

### Files Changed
- `config.h` — `useGameRV` field in Config struct, NVS load/save
- `bno086.h` — new methods, members, 20ms report interval
- `bno086.cpp` — complete rewrite: calibration control, GRV/RV selection, accuracy/stability/mag tracking
- `esp32-ahrs-gdl90.ino` — I2C pins, hardware comment, status JSON, calibration endpoints, settings
- `webui.h` — calibration UI card, Game RV toggle, accuracy display JS

### Calibration Procedure (3 sensors)

| Sensor | Procedure | Duration | Auto-calibrates? |
|--------|-----------|----------|------------------|
| Gyroscope | Set device on flat surface, hold still | 2-3 sec | Yes, whenever stationary |
| Accelerometer | "Cube method" — 4-6 unique orientations, hold each ~1 sec | ~10 sec | Yes (dynamic cal) |
| Magnetometer | Rotate ~180° and back in each axis (roll, pitch, yaw) | ~6 sec | Yes, but disabled in flight mode |

Calibration status per sensor: 0=Unreliable, 1=Low, 2=Medium, 3=High.
Target: all sensors at 2 or 3 before saving DCD.

### Sensor Fusion Mode Comparison

| Aspect | ARVR Stab. RV (0x28) | ARVR Stab. GRV (0x29, default) |
|--------|-------------------------------|----------------------------------|
| Pitch/Roll accuracy | 1.5-2.5° | 1.0-2.5° (slightly better) |
| Heading source | Magnetometer (noisy in cockpit) | Gyro-only (drifts, but clean) |
| Cockpit interference | Affected by avionics/engine | Immune |
| Calibration needed | Accel + Gyro + Mag | Accel + Gyro only |
| GPS heading fallback | Redundant (both available) | Sufficient (GPS primary) |

### BNO086 Accuracy Specifications

| Parameter | Value |
|-----------|-------|
| Pitch/Roll static | 1.0° (GRV) / 1.5° (RV) |
| Pitch/Roll dynamic | 2.5° |
| Heading static (RV with mag) | 2.0° |
| Heading drift (GRV, no mag) | < 0.5°/min typical |
| Power-on to first output | ~200ms |
| Fusion convergence after reset | 2-3 seconds |

### Remaining TODO (LOW priority)
- Z-axis tare function via WebUI for heading alignment
- Linear Acceleration report for G-load display
- Reset reason logging after watchdog recovery
- System Orientation FRS record instead of `invertRoll` flag

---

## ESP32-S3 Migration (completed, part of v0.5)

Migrated from ESP32-WROOM-32D to SparkFun Thing Plus ESP32-S3 (WRL-24408).

### Why ESP32-S3
- Native USB-C (no FTDI/CP2102 needed)
- More GPIO, more RAM
- SparkFun Thing Plus form factor with Qwiic connector built-in
- Same ESP32 Arduino core, minimal code changes

### Code Changes Applied

| Pin | ESP32-WROOM-32D | SparkFun Thing Plus ESP32-S3 |
|-----|-----------------|------------------------------|
| I2C SDA | GPIO 21 | GPIO 8 |
| I2C SCL | GPIO 22 | GPIO 9 |
| GPS RX | GPIO 16 | GPIO 16 (unchanged) |
| GPS TX | GPIO 17 | GPIO 17 (unchanged) |

### USB CDC Notes
- ESP32-S3 uses native USB CDC for serial — no external USB-serial chip
- `CDCOnBoot=1` must be set in board options
- `Serial.begin()` works the same, but boot messages may need a short delay
  for USB CDC enumeration (add `while(!Serial) { delay(10); }` if needed)
- WiFi and UDP remain identical — no changes to GDL90 output

### Case Compatibility
- SparkFun Thing Plus ESP32-S3 has the same Feather/Thing Plus form factor
- Mounting holes match existing case screw bosses (M2.5)
- USB-C port position may differ slightly — verify with actual board

---

## Stratux BNO086 Integration (separate project, Go Driver)

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

### Potential v0.5 plan:
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

| Sensor | Stratux (stock) | ESP32 AHRS (this project) | Stratux + BNO086 (v0.5) |
|--------|-----------------|---------------------------|-------------------------|
| IMU | ICM-20948 (SW Kalman) | BNO086 (HW fusion) | BNO086 (HW fusion) |
| Baro | BMP280 (±0.12 hPa) | BMP390 (±0.03 hPa) | BMP280 or BMP390 |
| GPS | u-blox (on Pi) | u-blox (on ESP32) | u-blox (on Pi) |
| ADS-B | SDR receiver | none | SDR receiver |
| AHRS quality | Good (SW) | Better (HW) | Best (HW, native) |
