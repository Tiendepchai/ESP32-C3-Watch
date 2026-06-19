# PINMAP and Strapping Review

**Board:** RoadPing C3 Mini (ESP32-C3-MINI-1-N4)
**Project:** `roadping-c3-mini-usbc-bq24074`
**Date:** 2026-06-16

---

## 1. Complete Pin Map (ESP32-C3-MINI-1-N4)

### 1.1 Module Pins

The ESP32-C3-MINI-1-N4 is a 42-pin castellated SMD module. Pin mapping below is based on the Espressif datasheet.

**Key pin assignments for this design:**

| GPIO | Function | Net | Pull | Strapping Risk |
|------|----------|-----|------|----------------|
| 0 | ADC1_CH0 | BAT_ADC (gated) | Divider | LOW=download mode — BSS138 gate OFF at reset = Hi-Z, safe |
| 1 | GPIO | GPIO_EN_ADC | 100k↓ GND | Not strapping — safe |
| 2 | ENC_A | ENC_A | 10k↑ 3V3 | HIGH needed for boot — 10k pull-up ensures HIGH |
| 3 | ENC_B | ENC_B | 10k↑ 3V3 | Not strapping |
| 4 | ENC_SW | ENC_SW | 10k↑ 3V3 | Not strapping |
| 5 | GPIO | BQ24074_CE | 10k↑ 3V3 | NOT strap on ESP32-C3 — safe |
| 6 | GPIO | BQ24074_CHG | 10k↑ 3V3 | Not strapping |
| 7 | GPIO | BQ24074_FAULT | 10k↑ 3V3 | Not strapping |
| 8 | I2C_SDA | I2C_SDA | 4.7k DNP | Samples for VDD_SPI — 10k pull-up → HIGH = 3.3V, safe |
| 9 | BOOT | BOOT_GPIO9 | Internal↑ | LOW=download — test pad, no external pulldown |
| 10 | I2C_SCL | I2C_SCL | 4.7k DNP | Not strapping |
| 18 | USB_D- | USB_DN | — | Not strapping |
| 19 | USB_D+ | USB_DP | — | Not strapping |
| 20 | UART_RX | UART_RX | — | Not strapping |
| 21 | UART_TX | UART_TX | — | Not strapping |

### 1.2 Peripheral Connections

| Peripheral | Pin | Net | GPIO |
|------------|-----|-----|------|
| SH1106 OLED | VDD | 3V3 | — |
| SH1106 OLED | GND | GND | — |
| SH1106 OLED | SCK | I2C_SCL | GPIO10 |
| SH1106 OLED | SDA | I2C_SDA | GPIO8 |
| PEC11R A | A | ENC_A | GPIO2 |
| PEC11R B | B | ENC_B | GPIO3 |
| PEC11R SW | SW1 | ENC_SW | GPIO4 |
| PEC11R COM | C | GND | — |
| JST-PH + | 1 | BAT_CONN+ | — |
| JST-PH - | 2 | GND | — |
| BQ24074 CE | 7 | GPIO5 | — |
| BQ24074 CHG | 10 | GPIO6 | — |
| BQ24074 FAULT | 11 | GPIO7 | — |
| BSS138 gate | GATE | GPIO1 | — |

### 1.3 Charger Status GPIOs

| GPIO | Net | Pull | Meaning |
|------|-----|------|---------|
| GPIO5 | CE_CHG_EN | 10k↑ 3V3 | Charger enable (active low) |
| GPIO6 | CHG_STATUS | 10k↑ 3V3 | Charging indicator (active low) |
| GPIO7 | FLT_STATUS | 10k↑ 3V3 | Fault indicator (active low) |

### 1.4 Test Pads

| Pad | Net | GPIO |
|-----|-----|------|
| TP_GND | GND | — |
| TP_3V3 | 3V3 | — |
| TP_TX | UART_TX | GPIO21 |
| TP_RX | UART_RX | GPIO20 |
| TP_SCL | I2C_SCL | GPIO10 |
| TP_BOOT | BOOT_GPIO9 | GPIO9 |

Pads: 1.3mm round, exposed copper, ENIG, 2.54mm pitch.

---

## 2. Old vs New Pin Map

| Function | Old (Super Mini) | New (MINI-1-N4) |
|----------|-----------------|-----------------|
| I2C_SDA | GPIO8 | GPIO8 |
| I2C_SCL | GPIO10 | GPIO10 |
| UART_TX | GPIO21 | GPIO21 |
| UART_RX | GPIO20 | GPIO20 |
| ENC_A | GPIO2 | GPIO2 |
| ENC_B | GPIO3 | GPIO3 |
| ENC_SW | GPIO4 | GPIO4 |
| BAT_ADC | GPIO0 | GPIO0 (gated) |
| BOOT | GPIO9 | GPIO9 (pull-up) |
| USB_D+ | N/A | GPIO19 |
| USB_D- | N/A | GPIO18 |
| CHG status | N/A | GPIO6 |
| FAULT status | N/A | GPIO7 |
| CE control | N/A | GPIO5 |
| BAT_GATE | N/A | GPIO1 |

---

## 3. Strapping Pin Analysis (ESP32-C3)

| GPIO | Function | Pull at Reset | Required State | Conflict? |
|:----:|----------|:-------------:|----------------|:---------:|
| 0 | BAT_ADC (gated) | BSS138 OFF = Hi-Z | HIGH for normal boot | No — FET isolated at reset |
| 2 | ENC_A (10k↑ 3V3) | 3.3V | HIGH for normal boot | No — pull-up ensures HIGH |
| 5 | CE_CHG_EN (10k↑ 3V3) | 3.3V | HIGH for 3.3V SPI | No — not strap on C3 |
| 8 | I2C_SDA (pull-up) | HIGH via pull-up | HIGH (suppress ROM log) | No — SDA undriven at reset |
| 9 | Unconnected (TP only) | Internal pull-up ~10k | HIGH for normal boot | **No conflict** — safety fix applied |

**Summary: No strapping conflicts remain.**

---

## 4. Risks and Bring-Up Checks

| Risk | Impact | Mitigation |
|------|--------|------------|
| GPIO0 divider voltage near V_IH at deep discharge | Could enter indeterminate strapping state | BSS138 gate OFF at reset = Hi-Z, isolated |
| GPIO2 toggles during reset (encoder rotation) | Strapping state captured incorrectly | 10k pull-up dominates encoder low |
| I2C bus contention during ESP32 reset | SDA/SCL held by OLED slave | I2C slaves release bus on power loss |

**Bring-up sequence:**
1. Power from USB, no LiPo, OLED disconnected
2. Measure VBAT (floating), SYS_RAW (~4.4V), 3V3 (3.3V±2%)
3. Verify GPIO0 >0.8V, GPIO9 ~3.3V
4. Connect OLED, verify I2C scan finds 0x3C/0x3D
5. Confirm normal boot and download mode entry via TP15
