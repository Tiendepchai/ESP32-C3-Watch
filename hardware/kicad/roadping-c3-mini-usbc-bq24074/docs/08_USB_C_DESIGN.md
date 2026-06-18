# USB-C Design — RoadPing C3 Mini

**Date:** 2026-06-16

---

## 1. Connector: TYPE-C-31-M-12

16-pin SMT USB-C receptacle. Mid-mount, board-edge.

| Pin | Net | Connection |
|-----|-----|------------|
| A1,B1,A12,B12 | GND | GND plane |
| A4,A9,B4,B9 | VBUS | F1 PTC → BQ24074 IN |
| A5 | CC1 | 5.1kΩ to GND |
| B5 | CC2 | 5.1kΩ to GND |
| A6,B6 | D+ | USBLC6-2SC6 → GPIO19 |
| A7,B7 | D- | USBLC6-2SC6 → GPIO18 |
| A8 | SBU1 | NC |
| B8 | SBU2 | NC |
| Shield | GND | RC (1M + 0.1µF) to GND |

## 2. CC Termination

Both CC1 and CC2: 5.1kΩ ±1% to GND. Identifies device as sink (UFP).
- USB 2.0 default: 500mA
- USB-C 1.5A: up to 1.5A (with 22kΩ Rp source)
- USB-C 3.0A: up to 3A (with 10kΩ Rp source)

No PD controller needed for this current range.

## 3. VBUS Protection

| Component | Function | Spec |
|-----------|----------|------|
| F1 — 1206L110SLWR | PTC resettable fuse | 1.1A hold, 2.2A trip |
| D1 — PESD5V0S1BA | TVS ESD diode | 5V VRWM, SOD-323 |

Topology: VBUS → D1 TVS → F1 PTC → BQ24074 IN

## 4. USB Data ESD: USBLC6-2SC6

| Parameter | Value |
|-----------|-------|
| Package | SOT-23-6 |
| Line cap | 0.6pF |
| ESD rating | ±15kV contact |
| VBUS bias | Pins 4,6 (required for clamp) |

**Routing:** USB-C D+ → USBLC6 pin1 → GPIO19. USB-C D- → USBLC6 pin3 → GPIO18.
Pins 2,5 → GND with individual vias.

**Series resistors:** 27Ω optional between USBLC6 and ESP32-C3 for impedance matching.

## 5. Differential Pair

90Ω differential microstrip on F.Cu, referenced to In1.Cu GND.

| Parameter | Value |
|-----------|-------|
| Trace width | 0.3mm |
| Trace spacing | 0.15mm |
| Dielectric height | 0.18mm |
| Length match | <2mm |
| Total length | <50mm |
| No vias | F.Cu only |

## 6. Layout Rules

- USBLC6 placed <5mm from USB-C receptacle
- Each GND pin (2,5) has dedicated via to GND plane
- No GND plane splits under diff pair
- ≥0.54mm clearance from other copper
- 45° chamfers, no 90° corners

## 7. BOM (USB-C Section)

| Ref | MPN | Function |
|-----|-----|----------|
| J1 | TYPE-C-31-M-12 | USB-C receptacle |
| R_CC1, R_CC2 | 5.1kΩ 1% 0603 | CC pulldowns |
| F1 | 1206L110SLWR | VBUS PTC |
| D1 | PESD5V0S1BA | VBUS TVS |
| U_ESD | USBLC6-2SC6 | D+/D- ESD |
