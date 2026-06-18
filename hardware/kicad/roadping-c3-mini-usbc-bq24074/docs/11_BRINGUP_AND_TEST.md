# Bring-Up and Test Plan — RoadPing C3 Mini

**Date:** 2026-06-16

---

## 1. Pre-Power Checks

1. Visual inspection: solder joints, bridges, polarity
2. DMM continuity: VBUS-GND (no short), VBAT-GND (no short), SYS_RAW-GND
3. DMM resistance: 3V3-GND (~kΩ range through LDO+caps)
4. Check J2 polarity: pin 1 = BAT_CONN+, pin 2 = GND
5. Current-limited PSU (100mA limit for first power)

## 2. Power-On Sequence

### Step 1: USB Power Only (no battery)
1. Connect USB-C to current-limited 5V PSU
2. Measure VBUS = 5V±5%
3. Measure SYS_RAW = ~4.4V (BQ24074 OUT from USB)
4. Measure 3V3 = 3.3V±2%
5. Check no excessive current (>50mA idle)

### Step 2: Battery Power Only (no USB)
1. Connect battery simulator (3.7V, current-limited)
2. Measure SYS_RAW = ~3.55V (BAT→OUT)
3. Measure 3V3 = 3.3V±2%
4. Remove simulator, verify SYS_RAW drops to 0V

### Step 3: USB + Battery
1. Connect USB + battery simulator at 3.3V
2. Verify CHG status on GPIO6
3. Measure charge current (~400-500mA input, ~250-350mA to battery)

## 3. USB Enumeration

1. Connect USB-C to PC
2. Check D+/D- differential signal (scope if available)
3. Device should enumerate as USB-SERIAL-JTAG (CDC ACM)
4. Verify `esptool.py chip_id` detects ESP32-C3

## 4. ESP32-C3 Boot

1. Monitor UART TX (GPIO21) — boot log?
2. Verify BOOT button (GPIO9 to GND) enters download mode
3. Flash firmware via `idf.py flash`
4. Verify serial console output via USB CDC-ACM

## 5. Charger Test

| Test | Condition | Expected |
|------|-----------|----------|
| No USB, no battery | GPIO6=HIGH, GPIO7=HIGH | — |
| USB only, no battery | GPIO6=HIGH, GPIO7=HIGH | No battery fault |
| USB + battery (3.3V) | GPIO6=LOW, GPIO7=HIGH | Charging |
| USB + battery (4.2V) | GPIO6=HIGH, GPIO7=HIGH | Charge complete |
| Remove battery | GPIO6=LOW, GPIO7=LOW | Battery absent |

## 6. Gated ADC Test

1. Gate OFF (GPIO1=LOW): read GPIO0 ADC → near 0
2. Gate ON (GPIO1=HIGH): wait 500ms, read ADC → VBAT×0.5
3. Compare with DMM on VBAT

## 7. Display Test

1. I2C scan → 0x3C or 0x3D
2. Send SH1106 init sequence
3. Display test pattern (all pixels on/off/checkerboard)

## 8. Encoder Test

1. Read GPIO2/3 state changes on rotation
2. Read GPIO4 state on press
3. Verify pull-up voltage on all three lines

## 9. Test Points

| Pad | Signal | Measurement |
|-----|--------|-------------|
| TP_GND | GND | 0V |
| TP_3V3 | 3V3 | 3.3V±2% |
| TP_TX | GPIO21 | 3.3V idle, toggle on boot |
| TP_RX | GPIO20 | Input, 3.3V (UART idle) |
| TP_SCL | GPIO10 | 3.3V idle (I2C) |
| TP_BOOT | GPIO9 | 3.3V idle, 0V when BOOT pressed |

## 10. Firmware Flashing

1. Hold BOOT (GPIO9 to GND)
2. Tap EN (power cycle)
3. Release BOOT
4. `idf.py -p /dev/ttyACM0 flash`
5. `idf.py -p /dev/ttyACM0 monitor`
