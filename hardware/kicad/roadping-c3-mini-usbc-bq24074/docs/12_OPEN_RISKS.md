# Open Risks — RoadPing C3 Mini

**Date:** 2026-06-16

---

## R1 — WQFN-20 Reflow (BQ24074RGTR)

**Risk:** 0.5mm pitch WQFN with exposed pad requires reflow assembly. Not hand-solderable without hot-air or hotplate.

**Mitigation:**
- Design stencil with 60-70% aperture on EP
- ≥4 thermal vias in pad
- Pre-heat board to 100°C before reflow
- X-ray inspection after assembly
- Consider pre-tinning EP + hot-air if hand-prototyping

**Owner:** Hardware | **Status:** OPEN

## R2 — USB TVS Clamp Voltage

**Risk:** PESD5V0S1BA clamps at ~9.8V (1A), exceeding BQ24074 abs max 6.5V on IN pin.

**Mitigation:**
- TVS is for ESD (ns pulses), not sustained OVP
- BQ24074 has internal 6.2V OVP for sustained overvoltage
- Accept as-is, document use case
- If issue arises, swap to PESD5V0S1BL (lower clamp) or TPSMA6L

**Owner:** Hardware | **Status:** ACCEPT (documented)

## R3 — BQ24074 ILIM Recalculation

**Risk:** ILIM=10kΩ gives ~100mA input limit, severely restricting charge current when system is active.

**Mitigation:**
- Calculate for 500mA input limit: R_ILIM = 1.0/0.5 × 1000 = 2.0kΩ
- Or 1A input limit: R_ILIM = 1.0kΩ
- Must decide before PCB order (value affects BOM)

**Owner:** Hardware | **Status:** OPEN — needs decision

## R4 — Gated ADC Settling Time

**Risk:** 470k+470k divider + 100nF creates ~106ms RC time constant. Need >500ms settling before ADC read.

**Mitigation:**
- Firmware: GPIO1 HIGH → 500ms delay → ADC read → GPIO1 LOW
- If too slow, reduce filter cap to 10nF (τ=10.6ms)
- Espressif ADC sampling time adjustment for 235kΩ source impedance

**Owner:** Firmware | **Status:** OPEN

## R5 — BSS138 Gate Drive at 3.0V VBAT

**Risk:** BSS138 Vgs(th) is 0.8-1.5V typ, but 3.0V gate drive may not fully enhance the FET when source is at divider bottom (near GND, Vgs ~ 3.3V from GPIO1).

**Mitigation:**
- GPIO1 drives 3.3V, BSS138 source is GND → Vgs = +3.3V (well above Vth)
- Rds(on) at Vgs=3.3V: ~3.5Ω, negligible in 940kΩ divider leg
- No issue: BSS138 confirmed suitable

**Owner:** — | **Status:** ACCEPT

## R6 — ESP32-C3-MINI-1 Footprint Unknown

**Risk:** No verified KiCad footprint for the 42-pin MINI-1-N4 module. Previous design uses Super Mini through-hole footprint — entirely different.

**Mitigation:**
- Create footprint from Espressif datasheet mechanical drawing
- Verify pin 1 location, pitch, row spacing, body dimensions
- Order test coupon before full PCB run

**Owner:** Hardware | **Status:** OPEN — needs footprint creation

## R7 — No NTC on Battery

**Risk:** BQ24074 TS pin fixed with 10k+10k divider. No battery temperature monitoring. Charging continues even if battery overheats.

**Mitigation:**
- BQ24074 die thermal regulation (125°C) provides secondary protection
- 5h safety timer prevents indefinite charging
- Phase 2: add NTC thermistor in battery pack and route to TS pin

**Owner:** Hardware | **Status:** ACCEPT (Phase 1)

## R8 — USB-C Mechanical Retention

**Risk:** TYPE-C-31-M-12 is mid-mount SMT without through-hole posts. Mechanical retention depends entirely on solder.

**Mitigation:**
- Connector has 2 SMT ground posts for additional strength
- If insufficient, switch to TYPE-C-31-M-12B or through-hole post variant
- Apply strain relief on cable side

**Owner:** Mechanical | **Status:** OPEN — verify on first article

## R9 — ADC Calibration Required

**Risk:** ESP32-C3 ADC has ±10% uncalibrated accuracy. Battery percentage will be inaccurate without calibration.

**Mitigation:**
- Phase 1: implement 2-point calibration (3.0V and 4.2V) with bench PSU
- Store calibration factor in NVS
- Phase 2: MAX17048 eliminates ADC entirely

**Owner:** Firmware | **Status:** OPEN (Phase 1) / DEFERRED (Phase 2)
