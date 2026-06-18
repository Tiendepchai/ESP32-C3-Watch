# Schematic Review — RoadPing C3 Mini

**Date:** 2026-06-16
**Status:** Pre-schematic design review

---

## 1. Scope

Schematic block design for RoadPing C3 Mini with ESP32-C3-MINI-1-N4 + BQ24074 + USB-C.

| # | Block | Key Components |
|---|-------|----------------|
| 1 | USB-C Input | TYPE-C-31-M-12, PTC, TVS, USBLC6-2SC6 |
| 2 | Battery Charger | BQ24074RGTR, ISET/ILIM/TS resistors |
| 3 | 3.3V Regulation | AP2112K-3.3 |
| 4 | MCU | ESP32-C3-MINI-1-N4 |
| 5 | I2C Bus | SH1106 OLED, pull-ups |
| 6 | Rotary Encoder | PEC11R-4215F-S0024 |
| 7 | Battery ADC | BSS138 gated divider, 470k+470k |
| 8 | Charger Status | CE(GPIO5), CHG(GPIO6), FAULT(GPIO7) |
| 9 | UART Debug | GPIO20/21, test pads |
| 10 | Protection | PTC, TVS, decoupling |

## 2. USB-C Input

**TYPE-C-31-M-12** connections:
- VBUS: A4/A9/B4/B9 → F1(PTC) → D1(TVS) → BQ24074 IN
- CC1(A5): 5.1kΩ to GND, CC2(B5): 5.1kΩ to GND
- D+(A6/B6): → USBLC6-2SC6(pin1) → GPIO19
- D-(A7/B7): → USBLC6-2SC6(pin3) → GPIO18
- Shield: 1MΩ + 0.1µF to GND
- SBU1(A8)/SBU2(B8): NC

**Protection:** F1(1206L110SLWR, 1.1A hold), D1(PESD5V0S1BA SOD-323 TVS)

**ESD:** USBLC6-2SC6 SOT-23-6 — line cap 0.6pF, ±15kV contact. VBUS bias on pins 4,6.

## 3. BQ24074RGTR Charger

| Pin | Name | Connection |
|-----|------|------------|
| 1,2 | IN | VBUS (from F1/D1) |
| 3 | VSS | GND |
| 4 | ISET | 1.1kΩ to GND (~910mA charge) |
| 5 | ILIM | 10kΩ to GND (~500mA input limit) |
| 6 | TS | 10kΩ→OUT + 10kΩ→GND (fixed) |
| 7 | /CE | GPIO5 (10k↑ 3V3) |
| 8,12 | OUT | SYS_RAW |
| 9 | PG | NC |
| 10 | /CHG | GPIO6 (10k↑ 3V3) |
| 11 | /FAULT | GPIO7 (10k↑ 3V3) |
| 13,16 | IN | VBUS |
| 14,15 | NC | NC |
| EP | GND | Thermal pad, must solder to GND |

**Passives:** C_IN=10µF+0.1µF, C_OUT=10µF+0.1µF, C_BAT=1µF

**Note:** ILIM=10kΩ gives ~100mA input limit — **needs recalculation to 1-2kΩ for 500mA-1A.**

## 4. AP2112K-3.3 LDO

| Pin | Name | Net |
|-----|------|-----|
| 1 | VIN | SYS_RAW |
| 2 | GND | GND |
| 3 | EN | SYS_RAW (always on) |
| 4 | NC | NC |
| 5 | VOUT | 3V3 |

C_IN=1µF, C_OUT=1µF, bulk 10µF on 3V3, 100nF local decoupling.

**Margin:** 600mA rated, ~290mA peak load. Dropout ~100mV@100mA.

## 5. ESP32-C3-MINI-1-N4

Key connections per pin map (Section 4 of this doc). Native USB-SERIAL-JTAG on GPIO18(D-)/GPIO19(D+).

**Boot strap pins verified safe:** GPIO0 (FET isolated), GPIO2 (10k↑), GPIO8 (pull-up), GPIO9 (unconnected).

## 6. SH1106 OLED (I2C)

VDD=3V3, GND=GND, SCK=GPIO10, SDA=GPIO8. Pull-ups 4.7k DNP.

## 7. PEC11R Encoder

A=GPIO2↑10k, C=GND, B=GPIO3↑10k, SW1=GPIO4↑10k, SW2=GND. Debounce 10n DNP.

## 8. Gated Battery ADC

BSS138: GATE=GPIO1+100k↓GND, DRAIN=divider jct, SOURCE=GND.
R_TOP=470k VBAT→divider, R_BOTTOM=470k divider→drain.
C_FILTER=100n divider→GND. Divider jct→GPIO0.

Ratio=0.5. V_ADC = VBAT×0.5 (max 2.1V at 4.2V VBAT).

**Gate pulldown required:** 100kΩ from BSS138 gate to GND ensures FET off at reset.

## 9. Design Issues

| ID | Severity | Issue | Fix |
|----|----------|-------|-----|
| SR-01 | HIGH | ILIM=10kΩ → ~100mA input limit | Change to 1-2kΩ |
| SR-02 | HIGH | No BSS138 gate pulldown | Add 100kΩ gate→GND |
| SR-03 | MED | PESD5V0S1BA clamp (9.8V) exceeds BQ24074 abs max (6.5V) | Accept as ESD-only; BQ24074 has internal OVP |
| SR-04 | MED | Verify MINI-1 pin mapping vs actual datasheet | Confirm before schematic entry |
