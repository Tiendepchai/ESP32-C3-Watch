# RoadPing C3 Mini - USB-C + BQ24074

Production-oriented KiCad design for a compact ESP32-C3 board.

## Key Components

- **ESP32-C3-MINI-1-N4** — RISC-V Wi-Fi/BLE SoC module
- **BQ24074** — Linear battery charger with power-path management
- **Single USB-C port** — Charge battery + flash firmware
- **SH1106 OLED** — Connected via I2C on GPIO 0/1 (or alternative)

## Design Directives

- Power path: USB-C 5V -> BQ24074 -> 3.3V LDO -> ESP32-C3 + peripherals
- LiPo/Li-ion battery management with NTC thermistor monitoring
- Single USB-C for both charging and UART/JTAG flashing
- Compact form factor (< 50 x 25 mm)
