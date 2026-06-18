# Architecture Decision Record: RoadPing C3 Mini

**Date:** 2026-06-16
**Board:** RoadPing C3 Mini (roadping-c3-mini-usbc-bq24074)
**Status:** DRAFT (pre-schematic)

---

## 1. Overview

RoadPing C3 Mini is a compact iOS ANCS notification display with SH1106 OLED, PEC11R encoder, and LiPo battery power. This document records the five highest-impact architecture decisions.

### Design Goals (ranked)
- P0: Single USB-C port for charge + flash/debug
- P0: Charge-and-run (power-path) on 1S LiPo
- P0: Native USB-SERIAL-JTAG (no external UART bridge)
- P1: Board area < 4000 mm² (80×50 mm target)
- P1: BLE ANCS push notification display
- P2: Battery voltage monitoring

## 2. Decision 1: MCU Selection — ESP32-C3-MINI-1-N4

| Parameter | ESP32-C3-MINI-1-N4 | ESP32-S3-MINI-1-N8 | ESP32-PICO-V3-ZERO |
|---|---|---|---|
| Core | Single RISC-V 160 MHz | Dual Xtensa LX7 240 MHz | Dual Xtensa LX6 240 MHz |
| Native USB | USB-SERIAL-JTAG | USB OTG | USB OTG |
| BLE | BLE 5.0 | BLE 5.0 | Classic BT + BLE 4.2 |
| Flash/PSRAM | 4MB / none | 8MB / 2MB | 4MB / 2MB |
| Module area | 18.0×22.5 mm | 18.0×25.5 mm | 21.0×21.0 mm |
| GPIO available | 14 | 26 | 27 |
| Price (1k) | ~$2.50-3.00 | ~$3.50-4.50 | ~$4.00-5.00 |

**Rationale:** Native USB-SERIAL-JTAG eliminates external UART bridge. Sufficient compute for ANCS + display. Smallest footprint. Adequate GPIO count (12 used of 14). Lower power than S3/PICO.

## 3. Decision 2: Power-Path Charger — BQ24074RGTR

Replaces TP4056 module. True power-path management (DPPM):
- USB present → system powered from USB, battery charged
- USB removed → seamless switch to battery (<20µs)
- No discrete ORing diodes or P-MOSFET required

| Feature | TP4056 (EVT) | BQ24074 (This design) |
|---|---|---|
| Power path | None | True DPPM |
| Package | THT module | WQFN-20 (3.5×3.5 mm) |
| PCB area | ~600 mm² | ~60 mm² |
| Charge current | 1A (R_PROG) | 1.5A max (ISET) |
| Status outputs | Single CHG LED | CHG + FAULT open-drain |
| NTC monitoring | None | TS pin (JEITA) |
| Safety timer | None | 5h default |

**ISET=1.1kΩ → ~910mA charge, ILIM=10kΩ → ~500mA input limit. TS fixed 10k+10k divider (no NTC).**

### Power Tree
```
USB-C VBUS (5V) → PTC F1 (1206L110SLWR) → TVS D1 (PESD5V0S1BA)
  → BQ24074 IN → BQ24074 OUT (SYS_RAW) → AP2112K-3.3 → 3V3 rail
               → BQ24074 BAT → JST-PH LiPo
```

## 4. Decision 3: USB-C Input — TYPE-C-31-M-12

Native USB-C receptacle with CC 5.1kΩ pulldowns (sink). USBLC6-2SC6 ESD on D+/D-.

```
USB-C D+ → USBLC6-2SC6 → opt 27Ω R → GPIO19 (USB_D+)
USB-C D- → USBLC6-2SC6 → opt 27Ω R → GPIO18 (USB_D-)
```

## 5. Decision 4: Gated Battery ADC (BSS138)

470k+470k divider gated by BSS138 N-FET. Gate HIGH → divider enabled, ADC reads VBAT×0.5. Gate LOW → <1µA leakage. GPIO1 gate control.

**Phase plan:** Phase 1 = gated ADC, Phase 2 = MAX17048 fuel gauge (future).

## 6. Decision 5: 4-Layer Stackup

F.Cu / In1.Cu (GND plane) / In2.Cu (PWR) / B.Cu. 1.6mm, ENIG. USB D+/D- 90Ω differential on F.Cu ref In1.Cu.

## 7. GPIO Allocation

| GPIO | Function | GPIO | Function |
|---|---|---|---|
| 0 | BAT_ADC (ADC1_CH0) | 6 | BQ24074_CHG (input) |
| 1 | GPIO_EN_ADC (BSS138 gate) | 7 | BQ24074_FAULT (input) |
| 2 | ENC_A | 8 | I2C_SDA |
| 3 | ENC_B | 9 | BOOT (unconnected) |
| 4 | ENC_SW | 10 | I2C_SCL |
| 5 | BQ24074_CE (output) | 18 | USB_D- | 19 | USB_D+ | 20 | UART_RX | 21 | UART_TX |

Total: 15 GPIOs used.

## 8. Schematic Fragments

### USB-C Input
```
J1: A1/A12/B1/B12=GND, A4/A9/B4/B9=VBUS, A5=CC1→5.1k→GND, B5=CC2→5.1k→GND
A6/B6=D+→USBLC6→GPIO19, A7/B7=D-→USBLC6→GPIO18
Shield: 1MΩ + 0.1µF to GND
```

### BQ24074
```
U1: IN(1,2)=VBUS, OUT(8)=SYS_RAW, BAT(12,13)=BAT_CONN+
ISET(4)=1.1k→GND, ILIM(5)=10k→GND, TS(6)=10k→OUT + 10k→GND
/CE(7)=GPIO5↑10k, CHG(10)=GPIO6↑10k, FAULT(11)=GPIO7↑10k
```

### AP2112K
```
U2: VIN(1)=SYS_RAW, EN(3)=SYS_RAW, VOUT(5)=3V3
```

### Gated ADC
```
Q1(BSS138): GATE=GPIO1+100k→GND, DRAIN=divider jct, SOURCE=GND
R_TOP=470k VBAT→divider, R_BOTTOM=470k divider→Q1 drain
C_FILTER=100n divider→GND, divider→GPIO0
```

### SH1106
```
DS1: VDD=3V3, GND=GND, SCK=GPIO10, SDA=GPIO8
Pull-ups: 4.7k DNP
```

### PEC11R
```
S1: A=GPIO2↑10k, C=GND, B=GPIO3↑10k
SW: 1=GPIO4↑10k, 2=GND
Debounce: 10n DNP
```

### UART Debug
```
TP_TX=GPIO21, TP_RX=GPIO20, TP_GND=GND, TP_3V3=3V3
Pads: 1.3mm round, ENIG, 2.54mm pitch
```
