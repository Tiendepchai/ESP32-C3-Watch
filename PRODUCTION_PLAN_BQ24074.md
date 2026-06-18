# RoadPing-C3 Mini USB-C BQ24074 — Production Plan (Full)

> **Project:** `hardware/kicad/roadping-c3-mini-usbc-bq24074/`  
> **Key IC:** BQ24074RGTR (power-path charger) + ESP32-C3-MINI-1-N4 + USB-C native  
> **Board:** 4-layer, 40×50mm, all 83 SMD on Top  
> **Date:** 2026-06-17  
> **Status:** CAD_REPAIR_IN_PROGRESS — schematic ERC not clean, PCB/parity not closed, new LDO decoupling caps not yet placed/routed on PCB  
> **Firmware:** NOT migrated (still on Super Mini pinout)

---

## MỤC LỤC

- [Phase 0: Project Overview & Health](#phase-0-project-overview--health)
- [Phase 1: PCB Closure — KiCad GUI](#phase-1-pcb-closure--kicad-gui)
- [Phase 2: BOM Lock & Sourcing](#phase-2-bom-lock--sourcing)
- [Phase 3: Fabrication Package](#phase-3-fabrication-package)
- [Phase 4: Firmware Migration](#phase-4-firmware-migration)
- [Phase 5: Validation & Test](#phase-5-validation--test)
- [Phase 6: Production Release](#phase-6-production-release)

---

## PHASE 0: PROJECT OVERVIEW & HEALTH

### Current Status

```
┌────────────────────────────────────────────────────────────┐
│                    ROADPING-C3 BQ24074                      │
│  Schematic: ❌ DRAFT, ERC not clean (156 violations)        │
│  PCB:       ❌ Openable after restore; DRC/parity not closed│
│  BOM:       ✅ 32 components, 19 LCSC PNs (62% MPN)        │
│  Firmware:  ❌ Still on Super Mini — NOT migrated           │
│  EMC:       ⚠️  Analyzer-only; must rerun after GND fill    │
│  Assembly:  ⚠️  Top-side intent only; not DVT released      │
└────────────────────────────────────────────────────────────┘
```

### Key Differences from EVT v2 (TP4056 → BQ24074)

| Aspect | EVT v2 (old) | BQ24074 (current) |
|--------|-------------|-------------------|
| Charger | TP4056 module (no power-path) | BQ24074RGTR (power-path, NTC, status outputs) |
| USB | External module via pin header | Native TYPE-C-31-M-12 + USBLC6-2SC6 ESD |
| ESP32 | Super Mini (third-party module) | ESP32-C3-MINI-1-N4 (official Espressif) |
| PCB | 2-layer, 19 SMD on bottom | 4-layer, 83 SMD all on top |
| Battery ADC | Direct divider (R6/R7) | Gated via BSS138 (saves power) |
| GPIOs | 8 used | 15 used (new: GPIO1/5/6/7 for charger) |
| BOM cost | ~$14/board | ~$9.10/board |

### 5 Unrouted Nets (BLOCKER)

> **Staleness note:** this table came from an earlier analyzer snapshot. Current DRC/parity cannot be accepted until KiCad can run full DRC with schematic parity on the active PCB.

| Net | Pins | Why missing |
|-----|------|-------------|
| Net-(J1-GND-PadA1) | J1 GND pads | USB-C shield GND daisy chain |
| Net-(J1-VBUS-PadA4) | J1 VBUS pads | VBUS from USB-C to BQ24074 |
| Net-(J1-SHIELD-PadSH) | J1 SHIELD pads | USB-C shield stitching |
| USB_DN_C3 | R13, R14 | USB DN termination resistors |
| ISET | R3, U1 | BQ24074 charge current set resistor |

**Fix:** Route these → fill GND zone → 0 DRC errors.

### Current CAD Evidence — 2026-06-18

| Item | Current evidence | Status |
|------|------------------|--------|
| Schematic ERC | `hardware/kicad/roadping-c3-mini-usbc-bq24074/ERC_AFTER_EN_LDO_CAPS_RESTOREPCB_2026-06-18.txt`: 156 violations | BLOCKER |
| PCB openability | KiCad Python `LoadBoard()` succeeds after restore from `.history`, 83 footprints | PARTIAL |
| DRC/parity | `kicad-cli pcb drc --schematic-parity` aborts with exit `134`; no report generated | BLOCKER |
| New LDO caps | `C_IN_LDO`, `C_OUT_LDO`, `C_BULK_3V3` added to schematic only | PCB_SYNC_REQUIRED |
| Release status | No fabrication/export approval | NO-GO |

### Current Layout Issues (from analyzers)

| Issue | Severity | Source |
|-------|----------|--------|
| 5 nets unrouted | 🔴 BLOCKER | PCB analyzer |
| No GND copper pour | 🔴 BLOCKER | EMC GP-002 |
| No fiducial markers | 🔴 BLOCKER | PCB FD-001 |
| BQ24074: No decoupling cap near U1 | 🔴 BLOCKER | EMC DC-002 |
| BQ24074: No thermal vias under U1 | 🟡 HIGH | PCB TV-001 |
| lib_footprint_mismatch (~27 items) | 🟡 HIGH | PCB |
| Stackup: 4 signal layers no ground plane | 🟡 HIGH | EMC SU-001 |
| SW1, SW2 missing silk labels | 🟡 HIGH | Silkscreen docs |
| R_CE, R_CHG, R_FLT no LCSC PN | 🟡 HIGH | BOM gaps |
| DS1 (OLED) no MPN/LCSC | 🟡 HIGH | BOM gaps |
| 3 rails missing PWR_FLAG | ℹ️ LOW | Schematic RS-001 |
| U2.EN, U3.CHIP_EN missing pull-up | ℹ️ LOW | Schematic PU-001 |

---

## PHASE 1: PCB CLOSURE — KiCad GUI

> **Skill: `kicad`** — Schematic/PCB analysis, DRC/ERC  
> **Tool:** KiCad 10 PCB Editor (GUI)  
> **Location:** `hardware/kicad/roadping-c3-mini-usbc-bq24074/roadping-c3-mini-usbc-bq24074.kicad_pcb`  
> **Target:** 0 DRC errors + 0 ERC errors + fabrication-ready PCB

### Step 1.0 — Open Project Correctly

Do **not** open `roadping-c3-mini-usbc-bq24074.kicad_pcb` directly in standalone PCB Editor.

Required workflow:

1. Open KiCad Project Manager.
2. Open `hardware/kicad/roadping-c3-mini-usbc-bq24074/roadping-c3-mini-usbc-bq24074.kicad_pro`.
3. Open Schematic Editor from Project Manager.
4. Run ERC and fix schematic issues first.
5. Use `Tools -> Update PCB from Schematic...` only from project context.

### Step 1.0A — ERC Repair Before Routing

Current ERC is not clean. Before PCB routing/fab work, fix all schematic connectivity issues caused by unsnapped wires, dangling labels, unconnected pins, or ambiguous power rails.

Minimum closure criteria:

```yaml
ERC errors: 0
Known intentional NC pins: marked with No Connect
Power rails: driven or documented with PWR_FLAG where required
U2.EN: tied to valid enable source
U3.CHIP_EN: pulled up through R13
No multiple-net-name conflicts
```

### Step 1.0B — Sync New LDO Decoupling Caps

The following schematic-level additions must be synced to PCB before any DVT/fab package:

```yaml
C_IN_LDO:   1uF 0603, SYS_RAW-to-GND, close to AP2112K VIN
C_OUT_LDO:  1uF 0603, 3V3-to-GND, close to AP2112K VOUT
C_BULK_3V3: 10uF 0603, 3V3-to-GND, near ESP32/OLED load path
```

Placement requirements:

- Short trace from cap pad to regulator/load rail.
- GND pad via to GND plane within 1 mm where practical.
- Do not place under inaccessible/tall modules.
- Refill zones after placement/routing.

### Step 1.1 — Route 5 Unrouted Nets

Use KiCad PCB Editor → route these manually:

```yaml
Net-(J1-GND-PadA1):   Connect all J1 GND pads (A1, A11, B1, B11, SHIELD)
                       Width: 0.3mm, layer: F.Cu
Net-(J1-VBUS-PadA4):   Connect J1 VBUS pads (A4, A9, B4, B9) → BQ24074 IN
                       Width: 0.5mm, layer: F.Cu
Net-(J1-SHIELD-PadSH): Shield stitching vias near J1 mounting posts
                       Width: 0.3mm
USB_DN_C3:             R13(27Ω) in series with USB_DN trace
USB_DP_C3:             R14(27Ω) in series with USB_DP trace
ISET:                  R3(1.1k) → U1 pin 13 (ISET)
```

**Verify:** View → Connectivity → "Highlight Net" — mỗi net phải highlight tất cả pads của nó.

### Step 1.2 — Fill GND Copper Pour

**Problem:** No GND zone on any layer. EMC GP-002: "No ground plane zones detected."

**Action:**
1. **Inner layer 1** (In1.Cu): **Full GND plane** — Place → Zone → draw board outline → net: `GND`
2. **Inner layer 2** (In2.Cu): **Full GND plane** — same
3. **F.Cu**: GND pour với clearance 0.3mm — để hở antenna keepout
4. **B.Cu**: GND pour với clearance 0.3mm

**Stackup sau fix:**
```
F.Cu  — Signal + GND pour
In1.Cu — GND PLANE     ← (new)
In2.Cu — GND PLANE     ← (new)
B.Cu  — Signal + GND pour
```

**Lợi ích:**
- Giải quyết 20 "unrouted GND daisy chains" (FreeRouting report)
- EMC: reference plane cho tất cả signal tracks
- Cung cấp return path ngắn cho high-speed (USB DP/DN)
- Tản nhiệt cho BQ24074 WQFN exposed pad

### Step 1.3 — Add Decoupling Caps Near U1 (BQ24074)

**Problem:** EMC DC-002 — "No decoupling cap found near U1."

**Action:** Thêm 2 caps CẠNH U1 (WQFN-20):
```yaml
C_new_1: 1µF 0603, VIN-to-GND, cạnh U1 pin 1 (IN)
C_new_2: 100nF 0603, VIN-to-GND, cạnh U1 pin 1 (IN)
Vias:    Sát pad GND của cap → xuống In1.Cu GND plane
```

**KiCad steps:** Place → Symbol → add to schematic → Annotate → Update PCB from Schematic → Place caps.

### Step 1.4 — Add Thermal Vias Under U1

**Problem:** PCB TV-001 — "Add thermal vias under U1 (need 5, have 0)."
**Risk:** BQ24074 WQFN-20 exposed pad sẽ không thoát nhiệt ở 910mA charge.

**Action:** Thêm 4-6 thermal vias trong exposed pad của U1:
```yaml
Via spec: 0.4mm drill, 0.7mm pad
Placement: 2×3 grid inside EP (2.7×2.7mm)
Connection: GND (EP is GND on BQ24074)
Tenting: UNTENTED (solder will fill during reflow)
```

**KiCad:** View → Via → đặt vào giữa EP footprint → Right-click → Properties → net=GND.

### Step 1.5 — Add Fiducial Markers

**Problem:** PCB FD-001 — "Add fiducial markers for pick-and-place."

**Action:** Add 3x `Fiducial:Fiducial_1mm_Mask3mm`:
```yaml
FID1: (13, 13)  — bottom-left
FID2: (47, 13)  — bottom-right
FID3: (13, 57)  — top-left
Exclude from BOM: Yes
```

### Step 1.6 — Resolve lib_footprint_mismatch Warnings

**Action:** Tools → Update Footprints from Library → chọn:
```
Standard (Update from library):
  C_0603_1608Metric, R_0603_1608Metric, SOT-23, SOT-23-5
  SOD-323, SOD-123, Fuse_1206_3216Metric
  WQFN-20-1EP, USB_C_Receptacle, PinHeader_1x02

Custom (Keep local):
  OLED_4PIN_VDD_GND_SCK_SDA_2.54MM
  MODULE_ESP32-C3-MINI-1-N4
  TP_Round_1.0mm
  XDCR_PEC11R-4215F-S0024
```

### Step 1.7 — Final DRC + ERC

**DRC Run:**
```bash
# Command-line check (if kicad-cli available)
kicad-cli pcb drc roadping-c3-mini-usbc-bq24074.kicad_pcb \
  --schematic-parity --output reports/drc_production.rpt
```

**DRC Target:**
```
ERROR gate:      0 errors, 0 unconnected
Full DRC:        0 errors, 0 warnings
Clearance:       0 (0.2mm min)
Shorts:          0
Silkscreen/pad:  0
Unrouted:        0
```

**ERC Target:**
```
Errors:   0
Warnings: accept PWR_FLAG warnings if documented
```

### Step 1.8 — Silkscreen Polish

Fix silkscreen warnings:
```
SW1: add "ENCODER" silk near right edge
SW2: add "BOOT" silk near TP pads
OLED: verify "VDD GND SCL SDA" silk at DS1 header
J2:  verify "BAT+" "BAT-" polarity silk
D2:  verify TVS cathode marking (bar)
Board name: add "RoadPing-C3" + "BQ24074" + "REV A" on F.SilkS
```

### Step 1.9 — Fix Decoupling Cap Via Proximity

EMC DC-003: C1, C2, C3, C4, C5, C6 via connection xa.

**Action:** Mỗi cap cần có via GND sát pad GND của cap — không quá 1mm. Dùng trace ngắn + rộng.

### Step 1.10 — Fix Stackup EMC Warning

EMC SU-001: "Adjacent signal layers: F.Cu, In1.Cu" — In1.Cu và In2.Cu đều là signal layers.

**Fix:** Chuyển In1.Cu và In2.Cu thành **GND planes**. Chỉ dùng F.Cu + B.Cu cho signal. Với 4-layer và GND planes ở inner, board sẽ có EMC tốt hơn 2-layer.

**Stackup mới:**
```yaml
F.Cu  — Signal + thin GND pour
In1.Cu — GND PLANE (solid, no breaks)
In2.Cu — GND PLANE (solid, no breaks)
B.Cu  — Signal + thin GND pour
```

---

## PHASE 2: BOM LOCK & SOURCING

> **Skills: `bom`, `lcsc`, `digikey`, `mouser`, `element14`, `datasheets`**  
> **Target:** 100% MPN coverage, 100% LCSC/DigiKey PN, verified stock

### Step 2.1 — Run BOM Analysis

**Skill: `bom`**

```bash
python3 <bom-skill>/scripts/bom_manager.py analyze \
  roadping-c3-mini-usbc-bq24074.kicad_sch --json --recursive
```

**Verify:**
- [ ] 32/32 components recognized
- [ ] All values parsed correctly
- [ ] Footprint assignments verified
- [ ] DNP policy decided (C9 debounce, I2C pull-ups)

### Step 2.2 — Fill Remaining Gaps (4 missing MPNs)

Current BOM gaps:

| Ref | Value | Missing | Skill |
|-----|-------|---------|-------|
| DS1 | SH1106 OLED | MPN + LCSC | Search `lcsc` for "SH1106 OLED 128x64 I2C" |
| R_CE, R_CHG, R_FLT | 10k 0603 | LCSC only (MPN fixed: RC0603FR-0710KL) | `lcsc` C98220 |
| SW2 | BOOT header 1x02 | MPN + LCSC | `lcsc` C492401 (same as EVT v2 J1) |
| TP_RX, TP_TX | Test pads | No MPN needed (PCB-only) | Skip |

**Skill: `lcsc`** — Search and verify:

```bash
python3 <lcsc-skill>/scripts/sync_datasheets_lcsc.py \
  --mpn-list mpns.txt --output ./datasheets
```

### Step 2.3 — Sync Datasheets

**Skill: `datasheets`**

Download datasheets cho tất cả parts có MPN:

| Part | MPN | Datasheet URL |
|------|-----|---------------|
| BQ24074 | BQ24074RGTR | https://www.ti.com/lit/ds/symlink/bq24074.pdf |
| ESP32-C3-MINI-1 | ESP32-C3-MINI-1-N4 | Espressif website |
| AP2112K-3.3 | AP2112K-3.3TRG1 | Diodes website |
| USBLC6-2SC6 | USBLC6-2SC6 | ST website |
| PESD5V0S1BA | PESD5V0S1BA | Nexperia website |
| BSS138 | BSS138 | ON Semi website |
| PEC11R | PEC11R-4215F-S0024 | Bourns |
| MF-SM013/250-2 | MF-SM013/250-2 | Bourns |
| TYPE-C-31-M-12 | TYPE-C-31-M-12 | HRO |

**Action:** Download PDFs to `datasheets/` and run:
```bash
python3 <datasheets-skill>/scripts/extract_datasheet.py datasheets/bq24074.pdf
python3 <datasheets-skill>/scripts/extract_datasheet.py datasheets/esp32-c3-mini-1.pdf
```

### Step 2.4 — Generate BOM Order Files

**Skill: `bom`**

```bash
# Export BOM tracking CSV
python3 <bom-skill>/scripts/bom_manager.py export \
  roadping-c3-mini-usbc-bq24074.kicad_sch \
  -o bom/production_bom.csv --recursive

# Generate per-distributor order files
python3 <bom-skill>/scripts/bom_manager.py order \
  bom/bom.csv --boards 5 --spares 2
```

### Step 2.5 — LCSC Order Verification

**Skill: `lcsc`**

Verify stock + pricing for all 19 LCSC PNs:

| LCSC Part | Part | Stock | Qty Order | Giá |
|:---------:|------|:----:|:---------:|:---:|
| C5673 | 1µF 0603 | Samsung | ~1.8M | 8 |
| C19702 | 10µF 0603 | Samsung | ~28M | 5 |
| C1591 | 100nF 0603 | Samsung | ~10M | 5 |
| C5261083 | PESD5V0S1BA | Nexperia | ~166k | 3 |
| C2687116 | USBLC6-2SC6 | ST | — | 2 |
| C720650 | PTC 1.1A | Bourns | — | 4 |
| C2907321 | TYPE-C-31-M-12 | HRO | — | 1 |
| C131337 | JST PH | JST | ~267k | 3 |
| C112239 | BSS138 | ON Semi | — | 3 |
| C105580 | 5.1k 0603 | Yageo | — | 5 |
| C137780 | 1.1k 0603 | Yageo | — | 2 |
| C105576 | 2.0k 0603 | Yageo | — | 2 |
| C98220 | 10k 0603 | Yageo | ~5.5M | 10 |
| C114622 | 470k 0603 | Yageo | — | 4 |
| C14675 | 100k 0603 | Yageo | — | 2 |
| C143790 | PEC11R | Bourns | ~797 | 1 |
| C54313 | BQ24074RGTR | TI | — | 3 |
| C23380830 | AP2112K-3.3 | Diodes | ~53k | 3 |
| C2838502 | ESP32-C3-MINI-1-N4 | Espressif | ~16k | 3 |

**Total LCSC order:** ~$7-10 + shipping ~$5-8 = **~$15**

### Step 2.6 — Special Order Parts (Non-LCSC)

| Part | Nguồn | Giá |
|------|-------|:---:|
| SH1106 OLED 128x64 I2C | AliExpress | ~$5 |
| F1/F2: 1206L110SLWR or MF-SM013/250-2 | DigiKey | ~$0.50 |
| DS1 OLED module jumper wires | Amazon | ~$2 |

**Skill: `digikey`** — Search F1/F2 alternatives:
```bash
python3 <digikey-skill>/search_digikey.py --mpn "1206L110SLWR"
```

---

## PHASE 3: FABRICATION PACKAGE

> **Skills: `jlcpcb`, `pcbway`, `kicad`**  
> **Target:** DVT candidate Gerbers + BOM + CPL. Do not label as production until all production gates pass.

### Step 3.1 — Re-export DVT Candidate Gerbers (After PCB Fixes)

**Skill: `kicad`**

```bash
# Export Gerbers
kicad-cli pcb export gerbers roadping-c3-mini-usbc-bq24074.kicad_pcb \
  --output fabrication/dvt_candidate/gerbers/ \
  --layers F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts \
  --no-x2 --no-protel-ext --subtract-soldermask

# Export Drill
kicad-cli pcb export drill roadping-c3-mini-usbc-bq24074.kicad_pcb \
  --output fabrication/dvt_candidate/drill/ \
  --format excellon --drill-origin absolute

# Export BOM + CPL for JLCPCB
kicad-cli pcb export pos roadping-c3-mini-usbc-bq24074.kicad_pcb \
  --output fabrication/dvt_candidate/cpl.csv --format csv --units mm --side front
```

### Step 3.2 — JLCPCB DVT Order Config

**Skill: `jlcpcb`**

```yaml
fabrication:
  base:
    pcb_qty: 5
    layers: 4
    thickness: 1.6mm
    material: FR-4 TG150
    surface_finish: HASL (lead-free)  # ENIG recommended for USB-C wear
    copper_weight: 1 oz all layers
    solder_mask: Green
    silkscreen: White
    min_track: 0.2mm
    min_hole: 0.3mm
    impedance: none (4-layer, no 90Ω needed — DP traces are short)

  pcb_assembly:
    type: Economic
    side: Top only
    stencil: Included (framed)
    bom_csv: bom/jlcpcb_bom.csv
    cpl_csv: fabrication/dvt_candidate/cpl.csv

  parts_not_on_lcsc:
    DS1: SH1106 OLED — hand solder after SMT reflow
    J2: JST-PH — hand solder (THT)
    SW1: PEC11R — hand solder (THT + shield posts)
    U3: ESP32-C3-MINI-1-N4 — verify LCSC C2838502
```

### Step 3.3 — Generate JLCPCB DVT BOM + CPL

```yaml
jlcpcb_bom.csv format:
  Comment,Designator,Footprint,LCSC Part #
  1uF,"C1,C2",0603,C5673
  10uF,C3,0603,C19702
  ...

jlcpcb_cpl.csv format:
  Designator,Mid X (mm),Mid Y (mm),Rotation (deg),Layer
  C1,44.0,25.0,0,Top
  C2,44.0,32.0,0,Top
  ...
```

### Step 3.4 — Option: PCBWay (if JLCPCB can't source parts)

**Skill: `pcbway`**

Nếu JLCPCB không hỗ trợ ESP32-C3-MINI-1-N4 (module) hoặc BQ24074 (WQFN), dùng PCBWay turnkey:
- PCBWay mua parts bằng MPN (không cần LCSC number)
- Giá cao hơn nhưng global sourcing
- Lead time: 7-12 ngày

---

## PHASE 4: FIRMWARE MIGRATION

> **Skill:** Manual coding (no automated skill)  
> **Location:** `firmware/components/*` + `firmware/main/board_pins.h`  
> **Target:** Firmware chạy trên ESP32-C3-MINI-1-N4 với BQ24074

### Step 4.1 — Update board_pins.h

**KHẤP:** Firmware hiện tại dùng **ESP32-C3 Super Mini**. Cần migrate sang **ESP32-C3-MINI-1-N4**.

**Thay đổi:**
```c
// OLD (Super Mini): GPIOs match yet but need to add new ones
#define BOARD_PIN_UART_TX 21
#define BOARD_PIN_UART_RX 20
#define BOARD_PIN_I2C_SDA 8
#define BOARD_PIN_I2C_SCL 10
#define BOARD_PIN_ENCODER_A 2
#define BOARD_PIN_ENCODER_B 3
#define BOARD_PIN_ENCODER_SW 4

// BQ24074 charger status pins (NEW — not in EVT v2)
#define BOARD_PIN_CHG_CE      5   // GPIO5: Charger Enable (active low pull-up)
#define BOARD_PIN_CHG_STATUS  6   // GPIO6: Charging status (active low)
#define BOARD_PIN_FLT_STATUS  7   // GPIO7: Fault status (active low)

// Gated battery ADC (NEW — BSS138 gate control)
#define BOARD_PIN_BAT_GATE    1   // GPIO1: BSS138 gate, HIGH = ADC active
#define BOARD_BAT_ADC_UNIT    1
#define BOARD_BAT_ADC_CHANNEL 0   // GPIO0 still ADC input
```

### Step 4.2 — Update board_config.h

```c
// OLD: BATTERY DIVIDER R6=220k/R7=100k → Vadc = Vbat * 3.2
// NEW: GATED DIVIDER R10=470k/R12=100k + BSS138 → Vadc = Vbat * 5.7

#define BOARD_FEATURE_BATTERY_ADC 1
#define BOARD_FEATURE_BAT_GATE    1   // NEW
#define BOARD_FEATURE_CHARGER_GPIO 1  // NEW

#define BAT_ADC_R_TOP_OHMS 470000U
#define BAT_ADC_R_BOTTOM_OHMS 100000U
#define BAT_ADC_DIVIDER_MULTIPLIER \
    (((float)BAT_ADC_R_TOP_OHMS + (float)BAT_ADC_R_BOTTOM_OHMS) / \
     (float)BAT_ADC_R_BOTTOM_OHMS)
// = (470k + 100k) / 100k = 5.7  → COMPILE-TIME CHECK THIS

#define BOARD_BATTERY_LOW_MV 3500U
#define BOARD_BATTERY_CRITICAL_MV 3300U

// ADC settling after BSS138 gate enable
#define BAT_ADC_GATE_SETTLE_MS 500U   // R10||R12 || C9 = ~3ms τ, 500ms safe
```

### Step 4.3 — Update battery.c

Add gated ADC logic:
```c
// In battery_read_mv():
// 1. GPIO1 = HIGH (enable BSS138 → divider connects to VBAT)
// 2. vTaskDelay(500ms) // ADC settling
// 3. adc_oneshot_read()
// 4. apply BAT_ADC_DIVIDER_MULTIPLIER (5.7)
// 5. GPIO1 = LOW (save power, divider disconnected)
```

### Step 4.4 — Add Charger Monitor Component

**New component:** `firmware/components/charger_bq24074/`

```c
// charger_bq24074.h
typedef enum {
    CHARGER_STATE_CHARGING,
    CHARGER_STATE_DONE,
    CHARGER_STATE_FAULT,
    CHARGER_STATE_DISABLED,
} charger_state_t;

esp_err_t charger_init(void);
charger_state_t charger_get_state(void);
esp_err_t charger_enable(bool enable);
```

**GPIO mapping:**
```c
GPIO5 (CE_CHG_EN) = output, active low — pull-up 10k (default = enabled)
GPIO6 (CHG_STATUS) = input, active low — 0 = charging
GPIO7 (FLT_STATUS) = input, active low — 0 = fault
```

### Step 4.5 — Update app_main.c

Thêm vào main loop:
```c
// Initialize BQ24074 charger monitor
charger_init();
charger_state_t chg_state = charger_get_state();

// In roadping_task loop:
// - Periodically check charger state (5s interval)
// - Update UI with charging status
// - If FAULT: display error and disable charging
```

### Step 4.6 — Sửa ADC Calibration Scale

ADC voltage divider change: **3.2 → 5.7**
```c
// OLD: Vbat = Vadc * 3.2
// NEW: Vbat = Vadc * (470k + 100k) / 100k = Vadc * 5.7
```

Với divider 470k+100k và 100nF C9, time constant τ = R_thevenin × C = (82k) × 100nF = 8.2ms.
- 5τ = 41ms settling
- Firmware wait: 500ms (safety margin 10×)

### Step 4.7 — I2C OLED Address (Same)

**Không thay đổi** — ESP32-C3-MINI-1-N4 vẫn dùng I2C GPIO8/GPIO10 cho SH1106.

### Step 4.8 — Host Test Verification

**Skill: Manual** — Run existing host tests + add new tests:

```bash
cd firmware/tests/build
cmake .. && make && ./roadping_host_tests
```

Add new test cases:
- [ ] battery.c: test new divider ratio 5.7
- [ ] battery.c: test ADC gate settling timer
- [ ] charger_bq24074.c: state machine unit tests
- [ ] notification_store.c: existing tests still pass

---

## PHASE 5: VALIDATION & TEST

> **Skills: `spice`, `emc`, `kicad`, `kidoc`**  
> **Target:** Physical boards pass all tests

### Step 5.1 — SPICE Simulation of Power Path

**Skill: `spice`**

Simulate BQ24074 power path behavior:
- [ ] BQ24074 startup sequence (VBUS ramp, SYS_RAW rise time)
- [ ] Load transient: 0mA → 150mA (BLE TX peak)
- [ ] USB removal: battery switchover <20µs
- [ ] Battery charge profile: trickle → CC → CV → done
- [ ] BQ24074 power dissipation at 910mA charge

```bash
python3 <spice-skill>/scripts/run_spice.py \
  --schematic analysis/bq24074_schematic.json \
  --subcircuit bq24074_power_path \
  --output analysis/spice/ \
  --simulator ngspice
```

### Step 5.2 — Final EMC Pre-Compliance

**Skill: `emc`**

After GND planes added:
```bash
python3 <emc-skill>/scripts/analyze_emc.py \
  --schematic analysis/bq24074_schematic.json \
  --pcb analysis/bq24074_pcb.json \
  --output analysis/bq24074_emc_final.json \
  --standard fcc-class-b
```

**Target:**
- Risk score: ≤ 30 (improvement from 23.5 after GND fill)
- GP-002: resolved (GND zones added)
- DC-002: resolved (decoupling caps near U1)
- SU-001: resolved (inner layers = GND planes)
- DC-003: resolved (via proximity fixed)

### Step 5.3 — Physical Bring-Up Sequence

Follow `docs/11_BRINGUP_AND_TEST.md`:

```yaml
Step 1: Visual inspection — shorts, polarity, solder bridges
Step 2: Power from USB (no LiPo) — DMM check:
         VBUS=5V, SYS_RAW=4.4V, 3V3=3.3V
Step 3: Flash firmware via UART (TP_TX, TP_RX)
Step 4: I2C scan → OLED at 0x3C
Step 5: Encoder rotation + press test
Step 6: Battery ADC calibration (DMM vs firmware)
         3.0V → 3.7V → 4.2V with bench PSU
         Store calibration factor in NVS
Step 7: BLE ANCS pairing with iPhone
Step 8: Navigation + notification UI test
Step 9: Power-path test:
         USB + battery → charging OK
         Remove USB → no brownout
         Battery only → 8µA BQ sleep
Step 10: Thermal — measure BQ24074 at 910mA charge
```

### Step 5.4 — Generate Documentation Package

**Skill: `kidoc`**

Generate production documentation:
```bash
python3 <kidoc-script> analyze --output docs/production/
```

Documents needed:
- [ ] Hardware Design Description (HDD)
- [ ] Test Report
- [ ] Manufacturing Transfer Package
- [ ] CE/FCC self-declaration

---

## PHASE 6: PRODUCTION RELEASE

> **Hard rule:** the current board is not production-ready. Production release is blocked until real assembled-board evidence, vendor lock, firmware migration, thermal, EMC/ESD, and fixture validation exist.

### Gate Checklist — MUST ALL PASS

```
□ GATE A: PCB
  □ 0 ERC errors before PCB update
  □ 0 DRC errors after zone refill
  □ 0 schematic/PCB parity issues
  □ 0 lib_footprint_mismatch
  □ All 5 unrouted nets connected
  □ GND planes on In1.Cu + In2.Cu
  □ 3 fiducial markers placed
  □ 4+ thermal vias under U1 (BQ24074)
  □ Decoupling caps near U1 (1µF + 100nF)
  □ AP2112K decoupling caps placed/routed (`C_IN_LDO`, `C_OUT_LDO`, `C_BULK_3V3`)
  □ Silkscreen: all labels readible
  □ Gerbers visually reviewed

□ GATE B: BOM
  □ 100% MPN coverage (32/32)
  □ 100% LCSC PN coverage (where available)
  □ BOM cost calculated
  □ LCSC order ready (19 parts, ~$15)
  □ Non-LCSC parts ordered (OLED, PTCs)

□ GATE C: FIRMWARE
  □ board_pins.h migrated to MINI-1-N4
  □ board_config.h updated (divider 5.7, gated ADC)
  □ battery.c gated ADC logic working
  □ charger_bq24074.c component created + tested
  □ ADC calibration routine implemented
  □ Compile-time divider ratio check added
  □ Host tests PASS

□ GATE D: VALIDATION
  □ 3+ boards assembled
  □ Power-path test: USB + battery OK
  □ Battery ADC accurate (±3% after calibration)
  □ BLE ANCS pairing + notifications working
  → Only now: GO_FOR_PRODUCTION
```

---

## TIMELINE

```
Week 1:  PHASE 1 (PCB Closure)
  Day 1-2: Route 5 nets + GND fill + thermal vias
  Day 2-3: Fiducials + decoupling caps + footprint fix
  Day 3-4: DRC/ERC → Gerber export → JLCPCB order
  Day 4-5: LCSC order + AliExpress module orders

Week 2-3: PHASE 4 (Firmware Migration)
  Day 6-8: board_pins.h + board_config.h migration
  Day 8-10: battery.c gated ADC + charger component
  Day 10-12: Host tests + code review

Week 3-4: (Chờ PCB + parts từ JLCPCB/LCSC/AliExpress)

Week 4-5: PHASE 5 (Validation)
  Day 21-25: Board assembly
  Day 25-28: Bring-up + calibration + BLE test
  Day 28-30: Power-path + thermal + EMC check

Week 6:  PHASE 6 (Production Release)
  Day 31-33: Fix any rev2 issues
  Day 34-35: Production order + documentation
```

---

## APPENDIX: SKILL REFERENCE

| Step | Skill | Purpose | Command/Usage |
|------|-------|---------|---------------|
| 1.1-1.10 | `kicad` | PCB layout, DRC, ERC, Gerber | `analyze_schematic.py`, `analyze_pcb.py`, `cross_analysis.py` |
| 2.1, 2.4 | `bom` | BOM analysis, export, order | `bom_manager.py analyze/export/order` |
| 2.2, 2.5 | `lcsc` | LCSC part search + datasheet | `sync_datasheets_lcsc.py` |
| 2.3 | `datasheets` | Datasheet extraction | `extract_datasheet.py` |
| 2.6 | `digikey` | DigiKey search | `search_digikey.py` |
| 3.2-3.3 | `jlcpcb` | JLCPCB order config | Template + BOM/CPL upload |
| 3.4 | `pcbway` | PCBWay alternative | Template |
| 5.1 | `spice` | Power-path simulation | `run_spice.py` |
| 5.2 | `emc` | EMC pre-compliance | `analyze_emc.py` |
| 5.4 | `kidoc` | Documentation generation | `kidoc` analysis |
