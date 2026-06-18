# BOM and Sourcing — RoadPing C3 Mini

Full BOM with 28 lines covering all components. See CSV block below or reference spreadsheet.

## Key Components

| Ref | MPN | Qty | Function |
|-----|-----|-----|----------|
| U1 | BQ24074RGTR | 1 | LiPo charger, power-path |
| U2 | AP2112K-3.3TRG1 | 1 | 3.3V LDO |
| U3 | ESP32-C3-MINI-1-N4 | 1 | WiFi/BLE module |
| D1 | USBLC6-2SC6 | 1 | USB D+/D- ESD |
| D2 | PESD5V0S1BA | 1 | VBUS TVS |
| Q1 | BSS138 | 1 | ADC gate FET |
| J1 | TYPE-C-31-M-12 | 1 | USB-C receptacle |
| J2 | B2B-PH-K-S | 1 | LiPo connector |
| SW1 | PEC11R-4215F-S0024 | 1 | Rotary encoder |
| DS1 | SH1106 OLED 128x64 | 1 | Display |
| F1,F2 | 1206L110SLWR | 2 | PTC fuses (VBUS + VBAT) |

## Excluded from Previous Design

- TP4056 module (→ replaced by BQ24074)
- D2 Schottky (→ no OR-ing needed)
- Q1 PMOS (→ removed, BQ24074 handles power-path)
- R6/R7 220k/100k (→ replaced by 470k/470k gated)

## Estimated Prototype Cost

| Category | Cost/board (qty 10) |
|----------|---------------------|
| ICs + modules | ~$4.50 |
| Discrete semi | ~$0.45 |
| Connectors | ~$1.20 |
| PTC + TVS | ~$0.80 |
| Passives | ~$0.35 |
| Electromechanical | ~$1.80 |
| **Total BOM** | **~$9.10** |
| PCB fabrication (4-layer) | ~$15-25 |
| **Total per board** | **~$25-35** |
