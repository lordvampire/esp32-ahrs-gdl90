# ESP32 AHRS GDL90 — Project Notes

## Current State (v0.4)

Working ForeFlight-compatible AHRS with dual-mode operation:
- **Standalone Mode**: Own WiFi AP, full GDL90 output
- **Companion Mode**: Auto-joins Stratux WiFi, broadcasts on Stratux subnet
- **Web Configuration**: Browser-based settings via `http://192.168.10.1`
- **Per-Sensor Config**: BNO086, BMP390 and GPS individually enable/disable via WebUI, persisted in NVS

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

## v0.5 — BNO086 Calibration & Sensor Fusion Optimization (planned)

**Goal**: Proper BNO086 calibration with persistence, optimized sensor fusion mode
for cockpit use, and WebUI calibration interface with guided procedure.

### Analysis Summary (from Datasheets)

**Current gaps** in the ESP32 AHRS code:
- Calibration data (DCD) is never saved → lost on every power cycle
- Dynamic calibration runs for magnetometer during flight → heading jumps
- Accuracy/status values from sensor reports are ignored
- No user-facing calibration procedure

**Key findings**:
- I2C/Qwiic is sufficient (< 1% bus usage at 10Hz, even 50Hz OK)
- Game Rotation Vector (0x08/0x29) is better for cockpit use than Rotation Vector (0x05/0x28)
  because magnetometer is unreliable in aircraft magnetic environment
- GPS heading already primary above 5kt → mag heading rarely needed
- Pitch/roll accuracy: 1.0-2.5° (comparable to certified backup attitude indicators)
- GRV heading drift: < 0.5°/min (acceptable for GPS-gap fallback)

### Calibration Procedure (3 sensors)

| Sensor | Procedure | Duration | Auto-calibrates? |
|--------|-----------|----------|------------------|
| Gyroscope | Set device on flat surface, hold still | 2-3 sec | Yes, whenever stationary |
| Accelerometer | "Cube method" — 4-6 unique orientations, hold each ~1 sec | ~10 sec | Yes (dynamic cal) |
| Magnetometer | Rotate ~180° and back in each axis (roll, pitch, yaw) | ~6 sec | Yes, but should disable in flight |

Calibration status per sensor: 0=Unreliable, 1=Low, 2=Medium, 3=High.
Target: all sensors at 2 or 3 before saving DCD.

### Planned Changes

#### HIGH Priority
1. **DCD persistence** (`bno086.cpp`)
   - Call `sh2_setDcdAutoSave(true)` at init
   - Call `saveCalibration()` after successful calibration
   - On `wasReset()`: reconfigure calibration settings (not just report)

2. **Accuracy monitoring** (`bno086.cpp/h`)
   - Read `getQuatAccuracy()` (0-3) on every update
   - Expose via `getAccuracy()` getter
   - Add to `/api/status` JSON response
   - Display on WebUI with color indicator

3. **Dynamic calibration control** (`bno086.cpp`)
   - Normal operation (flight): `setCalibrationConfig(SH2_CAL_ACCEL)` only
   - Calibration mode (ground): `setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG)`
   - Default (mag enabled) causes heading jumps in flight — must fix

4. **WebUI calibration page** (`webui.h`)
   - "Calibrate" button starts calibration mode
   - Step-by-step guide: Gyro → Accel → Mag
   - Live status display (0-3) for each sensor with color (red/yellow/green)
   - "Save" button calls `saveCalibration()` when all sensors at ≥2
   - "Cancel" restores normal (flight) calibration config

#### MEDIUM Priority
5. **Switch to ARVR Stabilized Game Rotation Vector (0x29)**
   - No magnetometer → no cockpit magnetic interference
   - Pitch/roll identical, heading drifts slowly
   - GPS heading is primary anyway
   - Fallback: `enableARVRStabilizedGameRotationVector(interval)`

6. **Increase report rate** to 50Hz
   - `BNO086_REPORT_INTERVAL_MS` from 100 → 20
   - Fresher data for 5Hz ForeFlight output, negligible I2C cost

7. **Magnetic Field report** (`0x03`) at 2Hz
   - Monitor mag calibration status independently
   - Essential during ground calibration to show progress

#### LOW Priority
8. Stability Classifier (`0x13`) at 1Hz — detect stationary for auto-DCD-save
9. Z-axis tare function via WebUI for heading alignment
10. Linear Acceleration report for G-load display
11. Reset reason logging after watchdog recovery
12. System Orientation FRS record instead of `invertRoll` flag

### Sensor Fusion Mode Comparison

| Aspect | Current: ARVR Stab. RV (0x28) | Proposed: ARVR Stab. GRV (0x29) |
|--------|-------------------------------|----------------------------------|
| Pitch/Roll accuracy | 1.5-2.5° | 1.0-2.5° (slightly better) |
| Heading source | Magnetometer (noisy in cockpit) | Gyro-only (drifts, but clean) |
| Cockpit interference | Affected by avionics/engine | Immune |
| Calibration needed | Accel + Gyro + Mag | Accel + Gyro only |
| GPS heading fallback | Redundant (both available) | Sufficient (GPS primary) |

### Files to Change
- `bno086.h` — report rate, accuracy getter, calibration mode flag
- `bno086.cpp` — DCD save, calibration config, accuracy reading, optional GRV switch
- `webui.h` — calibration page with guided procedure and live status
- `esp32-ahrs-gdl90.ino` — expose accuracy in status JSON, calibration API endpoints
- `config.h` — (optional) persist sensor fusion mode selection

### BNO086 Accuracy Specifications

| Parameter | Value |
|-----------|-------|
| Pitch/Roll static | 1.0° (GRV) / 1.5° (RV) |
| Pitch/Roll dynamic | 2.5° |
| Heading static (RV with mag) | 2.0° |
| Heading drift (GRV, no mag) | < 0.5°/min typical |
| Power-on to first output | ~200ms |
| Fusion convergence after reset | 2-3 seconds |

### Height Stack Clearance (BNO086 Qwiic ↔ ESP32)
```
BNO086 Qwiic connector hangs at  8.9 mm
ESP32 tallest component at        7.6 mm
Clearance:                        1.3 mm (Qwiic cable ~1mm flat → OK)
```

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
