# Layout Guidelines — RoadPing C3 Mini

**Date:** 2026-06-16

---

## 1. General Routing Rules

| Parameter | Value |
|-----------|-------|
| Min trace/space | 0.2mm (0.127mm fab capability) |
| Min via drill | 0.3mm |
| Min via pad | 0.6mm |
| Min annular ring | 0.15mm |
| Clearance copper-edge | 0.5mm |
| Default clearance | 0.2mm |

## 2. Power Routing

| Net | Width | Notes |
|-----|-------|-------|
| VBUS | 0.5mm | USB-C to BQ24074 IN |
| SYS_RAW | 0.5mm | BQ24074 OUT to AP2112K VIN |
| VBAT | 0.5mm | BQ24074 BAT to JST-PH |
| 3V3 | 0.4mm | Star topology, local branches |

## 3. USB D+/D- (90Ω Differential)

- Width: 0.3mm, spacing: 0.15mm
- F.Cu only, referenced to In1.Cu GND plane
- No vias on pair
- USBLC6 <5mm from connector
- Series resistors (27Ω) on IC side of USBLC6
- Length-matched <2mm

## 4. I2C (SCL/SDA)

- Width: 0.25mm
- Keep short (<100mm total)
- No stubs (daisy-chain preferred)
- Pull-ups near ESP32-C3 side

## 5. Battery ADC (BAT_ADC)

- Guard ring around divider resistors
- No digital traces parallel to ADC trace
- 100nF filter cap close to GPIO0 pin
- Minimal trace length from divider to GPIO0

## 6. Antenna Keepout

- ≥15mm from module antenna edge
- No copper — any layer, any shape
- No components in keepout zone
- DRC keepout rule on F.Cu, In1.Cu, In2.Cu, B.Cu

## 7. BQ24074 Thermal Pad

- WQFN-20 exposed pad (2.7×2.7mm)
- ≥4 thermal vias (0.3mm drill) in pad
- Connect to GND plane on In1.Cu
- Stencil aperture: 60-70% coverage on EP
- Thermal relief on pad vias if hand-solder

## 8. Decoupling

| Cap | Net | Location |
|-----|-----|----------|
| 10µF + 100nF | VBUS | Near BQ24074 IN pins |
| 10µF + 100nF | SYS_RAW | Near BQ24074 OUT pin |
| 1µF | SYS_RAW | Near AP2112K VIN |
| 1µF + 10µF | 3V3 | Near AP2112K VOUT |
| 100nF | 3V3 | Near each IC power pin |

## 9. Module GND Stitching

- ≥8 GND vias around ESP32-C3-MINI-1 perimeter
- Direct connection to In1.Cu GND plane
- 0.5mm trace or direct via-to-plane

## 10. Test Points

- 1.3mm round pad, exposed copper, ENIG
- 2.54mm pitch row
- Near board edge for probe access
- GND, 3V3, TX, RX, SCL, BOOT

## 11. Mounting Holes

- 4× M2 NPTH at corners
- 2.2mm drill, 5.0mm copper pad
- 1.4mm clearance from other copper
- GND connection if desired (ferrule optional)

## 12. Fabrication Notes

- 4-layer, 1.6mm, ENIG
- 1oz copper all layers
- FR4 TG 140-170°C
- Impedance coupon for 90Ω diff pair
- Green solder mask, white silkscreen
