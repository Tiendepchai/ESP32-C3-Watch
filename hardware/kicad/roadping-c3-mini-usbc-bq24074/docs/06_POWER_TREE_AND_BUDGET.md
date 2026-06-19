# Power Tree and Budget — RoadPing C3 Mini

**Date:** 2026-06-16

---

## 1. Power Tree

```
USB-C VBUS (5V)
  → PTC F1 (1206L110SLWR, 1.1A)
  → TVS D1 (PESD5V0S1BA)
  → BQ24074 IN
     |-- BQ24074 BAT → JST-PH LiPo (2000mAh)
     |-- BQ24074 OUT → SYS_RAW
                        → AP2112K-3.3 VIN/EN
                           → 3V3 rail (ESP32-C3, OLED, pull-ups)
```

## 2. Power Path Cases

1. **USB only, no battery** → BQ OUT from VBUS
2. **Battery only, no USB** → BQ OUT from BAT (8µA Iq sleep)
3. **USB + charging** → BQ OUT power-path, 910mA CC/CV
4. **USB + battery full** → BQ OUT power-path, done maintain
5. **USB removed suddenly** → BQ switches to BAT <20µs
6. **Battery low <3.0V** → Trickle charge 10%

## 3. Voltage Rails

| Rail | Source | Voltage | Notes |
|------|--------|---------|-------|
| VBUS | USB-C | 5V ±5% | After PTC/TVS |
| SYS_RAW | BQ24074 OUT | 3.5-5.0V | USB=~4.4V, BAT=3.0-4.2V |
| VBAT | LiPo | 3.0-4.2V | 3.7V nominal, 2000mAh |
| 3V3 | AP2112K-3.3 | 3.3V ±1.5% | 600mA rated |

## 4. Power Budget

### Active (BLE on, display on)
| Component | Current |
|-----------|---------|
| ESP32-C3 BLE active | 80mA |
| SH1106 OLED (50% pixels) | 20mA |
| Encoder pull-ups (3×10k) | 0.3mA |
| Charger status pull-ups (3×10k) | 0.3mA |
| **Total 3V3** | **~101mA** |
| LDO loss (89% eff @3.7V→3.3V) | ~13mA |
| **Total from SYS_RAW** | **~114mA** |

### Idle (BLE connected, display off)
| Component | Current |
|-----------|---------|
| ESP32-C3 modem-sleep | 5mA |
| SH1106 OLED sleep | 1µA |
| Encoder pull-ups | 0.3mA |
| **Total 3V3** | **~5.3mA** |

### Deep Sleep
| Component | Current |
|-----------|---------|
| ESP32-C3 deep sleep | 5µA |
| SH1106 OLED off | 1µA |
| AP2112K Iq | 55µA |
| BQ24074 Iq (battery save) | 8µA |
| Gated ADC (off) | <1µA |
| **Total from battery** | **~70µA** |

## 5. Runtime (2000mAh LiPo)

| Mode | Current | Runtime |
|------|---------|---------|
| Deep sleep | 70µA | ~2.9 years (theoretical) |
| Idle (BLE adv) | 0.5mA | ~166 days |
| Normal use | 50mA | ~40 hours |
| Active (continuous) | 150mA | ~13 hours |

## 6. BQ24074 Config

| Pin | Resistor | Value | Effect |
|-----|----------|-------|--------|
| ISET | R_ISET | 1.1kΩ | ~910mA charge |
| ILIM | R_ILIM | 10kΩ | ~500mA input limit |
| TS | R_TS1+R_TS2 | 10k+10k | Fixed mid-rail (no NTC) |
