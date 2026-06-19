# PCB Stackup and Constraints — RoadPing C3 Mini

**Date:** 2026-06-16
**Board:** 4-layer, ENIG, 1.6mm

---

## 1. Layer Stackup

| Layer | Name | Type | Thickness | Copper | Function |
|-------|------|------|-----------|--------|----------|
| — | Top Solder Mask | Mask | 0.010mm | — | Green LPI |
| 1 | F.Cu | Signal | 0.035mm | 1oz | Components, signals, USB diff pair |
| — | Prepreg | FR4 εr=4.5 | 0.180mm | — | Dielectric |
| 2 | In1.Cu | Plane | 0.035mm | 1oz | **Continuous GND plane** |
| — | Core | FR4 εr=4.5 | 1.080mm | — | Substrate |
| 3 | In2.Cu | Plane | 0.035mm | 1oz | Power distribution (SYS_RAW plane) |
| — | Prepreg | FR4 εr=4.5 | 0.180mm | — | Dielectric |
| 4 | B.Cu | Signal | 0.035mm | 1oz | Signals, GND pour, test pads |
| — | Bottom Solder Mask | Mask | 0.010mm | — | Green LPI |

**Total:** 1.60mm. Symmetrical stackup prevents warp.

## 2. Board Dimensions

| Parameter | Value |
|-----------|-------|
| Width | ~80mm |
| Height | ~50mm |
| Thickness | 1.6mm |
| Layers | 4 |
| Finish | ENIG |
| Copper | 1oz all layers |

## 3. Via Spec

| Parameter | Value |
|-----------|-------|
| Type | Through-hole only |
| Pad dia | 0.6mm |
| Drill | 0.3mm |
| Annular ring | 0.15mm |
| Tenting | Both sides |

## 4. Net Classes

| Net | Width | Class |
|-----|-------|-------|
| VBUS | 0.5mm | POWER_HIGH |
| SYS_RAW | 0.5mm | POWER_HIGH |
| BAT+ | 0.5mm | BATTERY_CHARGE |
| VBAT | 0.5mm | BATTERY_CHARGE |
| 3V3 | 0.4mm | POWER_3V3 |
| GND | 0.4mm | GND |
| USB_D+ / USB_D- | 0.3mm | USB_DIFF (90Ω diff) |
| I2C_SCL/SDA | 0.25mm | I2C |
| UART | 0.25mm | UART_DEBUG |
| Encoder | 0.25mm | SIGNAL |
| BAT_ADC | 0.25mm | ADC_ANALOG |

## 5. USB D+/D- Differential Pair

| Parameter | Value |
|-----------|-------|
| Target Zdiff | 90Ω |
| Model | Edge-coupled microstrip on F.Cu over In1.Cu |
| Trace width | 0.3mm |
| Trace spacing | 0.15mm |
| Length match | <2mm |
| Max length | <50mm |
| No vias on pair | F.Cu only |

## 6. Antenna Keepout

- ≥15mm clearance from module antenna edge
- No copper on any layer in keepout zone
- DRC keepout rule on all layers

## 7. BQ24074 Thermal Pad

- WQFN-20 exposed pad (2.7×2.7mm)
- ≥4×0.3mm thermal vias
- 300mm² GND copper on inner layer

## 8. Module GND Stitching

- ≥8 GND stitching vias around module perimeter
- Direct connection to In1.Cu GND plane
