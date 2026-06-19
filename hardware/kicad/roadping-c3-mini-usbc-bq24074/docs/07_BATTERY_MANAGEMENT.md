# Battery Management

**Date:** 2026-06-16

---

## 1. BQ24074 Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| Charge current | ~910mA | ISET=1.1kΩ |
| Input current limit | ~500mA | ILIM=10kΩ |
| TS | 10k+10k fixed | No NTC (Phase 1) |
| Pre-charge | 10% of I_CHG | V_BAT < 3.0V |
| Termination | C/10 | When I_CHG drops |
| Safety timer | 5h (default) | TMR floating |

## 2. Charger Status

| /CHG (GPIO6) | /FAULT (GPIO7) | Meaning |
|--------------|----------------|---------|
| Low | High | Charging |
| High | High | Done / disabled |
| Low | Low | Battery absent / temp fault |
| High | Low | Fault condition |

Both open-drain, 10kΩ pull-ups to 3.3V.

## 3. Gated ADC Circuit

```
VBAT → 470k R_TOP → +→ GPIO0 (BAT_ADC)
                     |
                   470k R_BOTTOM
                     |
                   DRAIN (BSS138)
                   GATE  ← GPIO1 (+100k↓ GND)
                   SOURCE → GND
                   
                     +→ 100n C_FILTER → GND
```

**Operation:** GPIO1 HIGH → BSS138 ON → divider enabled → ADC reads VBAT×0.5.
GPIO1 LOW → BSS138 OFF → <1µA leakage.

**Timing:** Gate HIGH → 500ms settling → ADC read (8-16 avg) → Gate LOW.
Sampling every 30-60s.

## 4. ADC Mapping

| VBAT | V_ADC (ratio 0.5) | ADC 12-bit (11dB atten) |
|------|-------------------|------------------------|
| 4.2V | 2.10V | ~2608 |
| 3.7V | 1.85V | ~2296 |
| 3.3V | 1.65V | ~2048 |
| 3.0V | 1.50V | ~1862 |

ADC range with 11dB attenuation: 0-2450mV. All values within linear range.

## 5. Component Values

| Component | Value | Notes |
|-----------|-------|-------|
| R_TOP | 470kΩ 1% 0603 | VBAT to divider |
| R_BOTTOM | 470kΩ 1% 0603 | Divider to FET drain |
| C_FILTER | 100nF X7R 0603 | ADC filter |
| Q1 | BSS138 SOT-23 | Vgs(th) 0.8-1.5V |
| R_GATE_PD | 100kΩ 0603 | Gate pulldown (failsafe) |

## 6. Phase Plan

| Phase | Feature | Status |
|-------|---------|--------|
| 1 | BQ24074 + gated ADC | **This design** |
| 2 | MAX17048 fuel gauge | Future (I2C, share bus) |
