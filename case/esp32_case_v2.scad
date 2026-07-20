// =========================================================
// ESP32-S3 Thing Plus Gehäuse v2 — Parametric OpenSCAD
// =========================================================
//
// Board:  SparkFun Thing Plus ESP32-S3 (58.42 × 22.86 mm)
// Lid:    SparkFun VR IMU BNO086      (25.4  × 30.48 mm) — screw mount
//         Adafruit BMP390 STEMMA QT   (25.4  × 17.78 mm) — screw mount
//
// v2 Changes vs v1:
//   - Asymmetric width (+4mm on connector side for Qwiic/LiPo)
//   - Screw bosses instead of clips (M2.5 at Feather holes)
//   - Cable slots for Qwiic and LiPo through connector wall
//   - Ledge only on non-connector side
// =========================================================

// ── Render selection ─────────────────────────────────────
// Set via command line: openscad -D 'part="base"' ...
// Values: "both" (default), "assembled", "base", "lid"
part = "both";

if (part == "base")      base();
if (part == "lid")       lid();
if (part == "assembled") view_assembled();
if (part == "both")      view_side_by_side();

// ── Board dimensions ─────────────────────────────────────

pcb_l   = 58.42;          // ESP32-S3 Thing Plus length (mm)
pcb_w   = 22.86;          // ESP32-S3 Thing Plus width  (mm)
pcb_t   = 1.6;            // standard PCB thickness

// ── Tolerances ───────────────────────────────────────────

tol     = 0.3;            // XY clearance per side

// ── Asymmetric width (connector side) ───────────────────

extra_w = 4.0;            // additional width on connector side

// ── Case geometry ────────────────────────────────────────

wall    = 1.6;            // wall thickness
floor_t = 1.6;            // floor plate thickness
cr      = 3.0;            // outer corner radius
inner_h = 14.0;           // internal cavity height in base

// ── PCB support ledge (non-connector side only) ─────────

ledge_w = 1.0;            // ledge width (inward from wall)
ledge_h = 2.0;            // ledge height above floor

// ── Screw bosses (M2.5 self-tapping) ───────────────────

boss_od = 5.5;            // boss outer diameter
boss_id = 2.2;            // pilot hole for M2.5
// Feather/Thing Plus mounting holes (from PCB origin)
hole_x1 = 2.54;
hole_x2 = 55.88;
hole_y1 = 2.54;
hole_y2 = 20.32;

// ── Connector cable slots (through connector-side wall) ─

qwiic_x      = 5.0;      // Qwiic center offset from USB-C end
qwiic_slot_w = 8.0;      // Qwiic slot width
lipo_x       = 14.0;     // LiPo center offset from USB-C end
lipo_slot_w  = 8.0;      // LiPo slot width
conn_slot_h  = 3.0;      // slot height

// ── USB-C cutout (closed opening in wall) ───────────────

usbc_w  = 10.0;           // opening width
usbc_h  = 7.0;            // opening height (PCB + connector + clearance)

// ── Lid ──────────────────────────────────────────────────

lid_t   = 1.6;            // top plate thickness
lip_h   = 2.0;            // snap-in lip depth
lip_gap = 0.2;            // clearance between lip and cavity
lip_w   = 1.0;            // lip shell wall thickness

// ── Corner feet ──────────────────────────────────────────

foot_r  = 2.0;            // foot pad radius
foot_h  = 1.0;            // foot pad height
strut_w = 0.8;            // connecting strut width
strut_off = 3.0;          // per-axis offset from box corner

// ── Lid USB extension (T-shape) ──────────────────────────

ext_w   = 14.0;           // channel width
ext_d   = 12.0;           // protrusion depth from lid edge
ext_h   = 6.0;            // channel height
ext_t   = 1.2;            // channel wall thickness
slot_w  = 2.0;            // cable slot width
slot_h  = 4.0;            // cable slot height
slot_sp = 6.0;            // spacing between slot centers

// ── Cooling vent slots (wave openings through floor) ─────

n_vents    = 4;            // number of vent openings
vent_w     = 2.0;          // slot width
vent_amp   = 1.5;          // wave amplitude
vent_margin = 13.0;        // margin from short walls (clear of bosses)

// ── BNO086 Board (SparkFun SEN-22857) ──────────────────
bno_l  = 30.48;           // BNO086 length (along X)
bno_w  = 25.4;            // BNO086 width  (along Y)

// BNO086 mounting holes: 24mm apart along X (measured)
bno_hole_x1 = 3.24;       // hole 1: X from board origin = (30.48-24)/2
bno_hole_x2 = 27.24;      // hole 2: X from board origin = 3.24 + 24
bno_hole_y  = 2.54;       // both holes at this Y (near one edge)

// BNO086 lid mounting
bno_standoff_h = 1.5;     // standoff height (clears backside SMD)
bno_boss_od    = 4.0;     // boss outer diameter (reduced for lip clearance)
bno_boss_id    = 2.0;     // pilot hole dia (M2 self-tapping)
bno_support_w  = 1.5;     // anti-rotation support width
bno_support_l  = 10.0;    // anti-rotation support length (along X)

// ── BMP390 Board (Adafruit 4816 STEMMA QT) ────────────
// Rotated 90° in case: short side along X, long side along Y
bmp_pcb_x  = 17.78;       // BMP390 along X (rotated)
bmp_pcb_y  = 25.4;        // BMP390 along Y (rotated)
sensor_gap = 2.0;          // board gap (8mm between nearest holes)

// BMP390 mounting holes: 20mm apart along Y (measured)
bmp_hole_x  = 2.76;       // both holes at this X (near BNO-facing edge)
bmp_hole_y1 = 2.7;        // hole 1: Y from board origin = (25.4-20)/2
bmp_hole_y2 = 22.7;       // hole 2: Y from board origin = 2.7 + 20

// BMP390 lid mounting (same hardware as BNO086)
bmp_boss_od    = 4.0;     // boss outer diameter (reduced for lip clearance)
bmp_boss_id    = 2.0;     // pilot hole dia (M2 self-tapping)
bmp_support_w  = 1.5;     // anti-rotation support width
bmp_support_l  = 8.0;     // anti-rotation support length (along Y)

// ── Resolution ───────────────────────────────────────────

$fn = 60;

// ═════════════════════════════════════════════════════════
// DERIVED DIMENSIONS — do not modify
// ═════════════════════════════════════════════════════════

cav_l    = pcb_l + 2 * tol;                    // cavity length
cav_w    = pcb_w + 2 * tol + extra_w;          // cavity width (asymmetric)
box_l    = cav_l + 2 * wall;                   // outer box length
box_w    = cav_w + 2 * wall;                   // outer box width
box_h    = floor_t + inner_h;                  // outer box height
cr_i     = max(cr - wall, 0.5);                // inner corner radius

// PCB origin in box coordinates
pcb_x0   = wall + tol;
pcb_y0   = wall + tol;                         // non-connector side

// PCB center Y (for USB-C and extension centering)
pcb_cy   = pcb_y0 + pcb_w / 2;

// Connector-side wall inner face Y
conn_wall_y = box_w - wall;

// Sensor positions (BNO086 + BMP390 side-by-side, centered as group)
bno_x0 = wall + (cav_l - bno_l - sensor_gap - bmp_pcb_x) / 2;
bno_y0 = wall + (cav_w - bno_w) / 2;
bmp_x0 = bno_x0 + bno_l + sensor_gap;
bmp_y0 = wall + (cav_w - bmp_pcb_y) / 2;

// USB-C notch bottom: at PCB bottom level
usbc_notch_z = floor_t + ledge_h;

// Connector slot bottom: at PCB top surface
conn_slot_z = floor_t + ledge_h + pcb_t;

// ═════════════════════════════════════════════════════════
// HELPER MODULES
// ═════════════════════════════════════════════════════════

// Rounded box — corner at origin, rounded XY edges only
module rbox(l, w, h, r) {
    translate([r, r, 0])
        linear_extrude(h)
            offset(r = r)
                square([l - 2*r, w - 2*r]);
}

// ═════════════════════════════════════════════════════════
// BASE
// ═════════════════════════════════════════════════════════

module base() {
    difference() {
        union() {
            _base_shell();
            _screw_bosses();
        }
        _usbc_cutout();
        _connector_cutouts();
        _vent_slots();
        _boss_holes();
    }
}

// Hollow shell with rounded corners
module _base_shell() {
    difference() {
        rbox(box_l, box_w, box_h, cr);
        translate([wall, wall, floor_t])
            rbox(cav_l, cav_w, box_h, cr_i);
    }
}

// PCB support ledge — non-connector side only
module _pcb_ledge() {
    translate([wall, wall, floor_t])
        cube([cav_l, ledge_w, ledge_h]);
}

// Screw bosses at Feather mounting hole positions
module _screw_bosses() {
    holes = [
        [pcb_x0 + hole_x1, pcb_y0 + hole_y1],
        [pcb_x0 + hole_x2, pcb_y0 + hole_y1],
        [pcb_x0 + hole_x1, pcb_y0 + hole_y2],
        [pcb_x0 + hole_x2, pcb_y0 + hole_y2]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], floor_t])
            cylinder(h = ledge_h, d = boss_od);
    }
}

// Pilot holes through bosses (subtracted in base())
module _boss_holes() {
    holes = [
        [pcb_x0 + hole_x1, pcb_y0 + hole_y1],
        [pcb_x0 + hole_x2, pcb_y0 + hole_y1],
        [pcb_x0 + hole_x1, pcb_y0 + hole_y2],
        [pcb_x0 + hole_x2, pcb_y0 + hole_y2]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], -0.01])
            cylinder(h = floor_t + ledge_h + 0.02, d = boss_id);
    }
}

// Cable slots through connector-side wall (Qwiic + LiPo)
module _connector_cutouts() {
    // Qwiic slot
    translate([pcb_x0 + qwiic_x - qwiic_slot_w / 2,
               conn_wall_y - 0.01,
               conn_slot_z])
        cube([qwiic_slot_w, wall + 0.02, conn_slot_h]);

    // LiPo slot
    translate([pcb_x0 + lipo_x - lipo_slot_w / 2,
               conn_wall_y - 0.01,
               conn_slot_z])
        cube([lipo_slot_w, wall + 0.02, conn_slot_h]);
}

// Wave-shaped cooling vent openings cut through the floor
module _vent_slots() {
    vl = cav_l - 2 * vent_margin;

    for (i = [1 : n_vents]) {
        // Space vents under the PCB area only
        y = pcb_y0 + pcb_w * i / (n_vents + 1);
        translate([wall + vent_margin, y, -0.01])
            linear_extrude(floor_t + 0.02)
                _wave2d(vl, vent_amp, vent_w);
    }
}

// 2D sine-wave strip (two full cycles)
module _wave2d(length, amp, thick) {
    n   = 60;
    dx  = length / n;
    top = [for (i = [0:n]) [i * dx,  amp * sin(i * 720 / n) + thick / 2]];
    bot = [for (i = [n:-1:0]) [i * dx, amp * sin(i * 720 / n) - thick / 2]];
    polygon(concat(top, bot));
}

// Four corner feet with connecting struts
module _feet() {
    corners = [[0, 0], [box_l, 0], [box_l, box_w], [0, box_w]];
    dirs    = [[-1, -1], [1, -1], [1, 1], [-1, 1]];

    for (i = [0 : 3]) {
        cx = corners[i][0];  cy = corners[i][1];
        dx = dirs[i][0];     dy = dirs[i][1];

        // Foot pad
        fx = cx + dx * strut_off;
        fy = cy + dy * strut_off;
        translate([fx, fy, 0])
            cylinder(h = foot_h, r = foot_r);

        // Connecting strut
        ang  = atan2(dy, dx);
        slen = sqrt(2) * strut_off;
        translate([cx, cy, 0])
            rotate([0, 0, ang])
                translate([0, -strut_w / 2, 0])
                    cube([slen, strut_w, foot_h]);
    }
}

// USB-C closed opening through wall, centered on PCB
module _usbc_cutout() {
    translate([-0.01, pcb_cy - usbc_w / 2, usbc_notch_z])
        cube([wall + 0.02, usbc_w, usbc_h]);
}

// ═════════════════════════════════════════════════════════
// LID
// ═════════════════════════════════════════════════════════

module lid() {
    difference() {
        union() {
            // Top plate
            rbox(box_l, box_w, lid_t, cr);

            // Inner snap-fit lip
            _lid_lip();

            // BNO086 mounting (screw bosses)
            _bno_bosses();
            _bno_support();

            // BMP390 mounting (screw bosses)
            _bmp_bosses();
            _bmp_support();
        }
        _bno_pilot_holes();
        _bmp_pilot_holes();
    }
}

// Thin-shell lip that slides into the base cavity
module _lid_lip() {
    ll = cav_l - 2 * lip_gap;
    lw = cav_w - 2 * lip_gap;
    lr = max(cr_i - lip_gap, 0.3);

    translate([wall + lip_gap, wall + lip_gap, lid_t - 0.01])
        difference() {
            rbox(ll, lw, lip_h + 0.01, lr);
            translate([lip_w, lip_w, -0.01])
                rbox(ll - 2 * lip_w, lw - 2 * lip_w,
                     lip_h + 0.03, max(lr - lip_w, 0.2));
        }
}

// T-shaped USB-C cable channel extending from one short side
// Centered on PCB center, not box center
module _usb_extension() {
    ey    = pcb_cy - ext_w / 2;              // Y offset to center on PCB
    end_r = ext_w / 2;                       // semicircle radius for rounded end

    difference() {
        union() {
            // Rectangular channel body
            translate([-ext_d, ey, 0])
                cube([ext_d + 0.01, ext_w, ext_h]);

            // Rounded end cap (half-cylinder)
            translate([-ext_d, pcb_cy, 0])
                linear_extrude(ext_h)
                    intersection() {
                        circle(r = end_r);
                        translate([-end_r * 2, -end_r])
                            square([end_r * 2, end_r * 2]);
                    }
        }

        // Hollow out rectangular body
        translate([-ext_d, ey + ext_t, ext_t])
            cube([ext_d + 0.02, ext_w - 2 * ext_t,
                  ext_h - ext_t + 0.01]);

        // Hollow out rounded end cap
        translate([-ext_d, pcb_cy, ext_t])
            linear_extrude(ext_h - ext_t + 0.01)
                intersection() {
                    circle(r = end_r - ext_t);
                    translate([-end_r * 2, -end_r])
                        square([end_r * 2, end_r * 2]);
                }

        // Cable routing slots through the curved end wall
        translate([-ext_d - end_r - 0.01,
                   pcb_cy - slot_sp / 2 - slot_w / 2,
                   ext_t])
            cube([end_r + 0.02, slot_w, slot_h]);
        translate([-ext_d - end_r - 0.01,
                   pcb_cy + slot_sp / 2 - slot_w / 2,
                   ext_t])
            cube([end_r + 0.02, slot_w, slot_h]);
    }
}

// BNO086 standoff bosses on lid inner face (2× mounting holes along X)
module _bno_bosses() {
    holes = [
        [bno_x0 + bno_hole_x1, bno_y0 + bno_hole_y],
        [bno_x0 + bno_hole_x2, bno_y0 + bno_hole_y]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], lid_t])
            cylinder(h = bno_standoff_h, d = bno_boss_od);
    }
}

// BNO086 pilot holes through bosses + lid plate (for screws from outside)
module _bno_pilot_holes() {
    holes = [
        [bno_x0 + bno_hole_x1, bno_y0 + bno_hole_y],
        [bno_x0 + bno_hole_x2, bno_y0 + bno_hole_y]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], -0.01])
            cylinder(h = lid_t + bno_standoff_h + 0.02, d = bno_boss_id);
    }
}

// Anti-rotation support on far Y edge of BNO086 (opposite to screw holes)
module _bno_support() {
    translate([bno_x0 + (bno_l - bno_support_l) / 2,
               bno_y0 + bno_w - bno_support_w - 0.5,
               lid_t])
        cube([bno_support_l, bno_support_w, bno_standoff_h]);
}

// BMP390 standoff bosses on lid inner face (2× mounting holes)
module _bmp_bosses() {
    holes = [
        [bmp_x0 + bmp_hole_x, bmp_y0 + bmp_hole_y1],
        [bmp_x0 + bmp_hole_x, bmp_y0 + bmp_hole_y2]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], lid_t])
            cylinder(h = bno_standoff_h, d = bmp_boss_od);
    }
}

// BMP390 pilot holes through bosses + lid plate
module _bmp_pilot_holes() {
    holes = [
        [bmp_x0 + bmp_hole_x, bmp_y0 + bmp_hole_y1],
        [bmp_x0 + bmp_hole_x, bmp_y0 + bmp_hole_y2]
    ];

    for (pos = holes) {
        translate([pos[0], pos[1], -0.01])
            cylinder(h = lid_t + bno_standoff_h + 0.02, d = bmp_boss_id);
    }
}

// Anti-rotation support on opposite short edge of BMP390
module _bmp_support() {
    translate([bmp_x0 + bmp_pcb_x - bmp_support_w,
               bmp_y0 + (bmp_pcb_y - bmp_support_l) / 2,
               lid_t])
        cube([bmp_support_w, bmp_support_l, bno_standoff_h]);
}

// ═════════════════════════════════════════════════════════
// VIEW MODULES
// ═════════════════════════════════════════════════════════

// Print layout — both parts flat, side by side
// Lid is mirrored so T-extension points away from base
module view_side_by_side() {
    base();
    translate([2 * box_l + 15, 0, 0])
        mirror([1, 0, 0])
            lid();
}

// Preview — assembled (lid on base, color-coded)
module view_assembled() {
    color("SteelBlue", 0.8)
        base();

    color("OrangeRed", 0.6)
        translate([0, 0, box_h])
            mirror([0, 0, 1])
                lid();
}
