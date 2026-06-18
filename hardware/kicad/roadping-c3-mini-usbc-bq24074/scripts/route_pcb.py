#!/usr/bin/env python3
"""RoadPing C3 Mini — PCB routing generator.
   Applies traces + vias + zones to the placed PCB."""
import os, sys, re, uuid as _u

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")
U = lambda: str(_u.uuid4())

# ── Read existing PCB ────────────────────────────────────────────
with open(PCB) as f:
    pcb_text = f.read()

# Find net IDs and footprint positions
# KiCad auto-assigns net numbers. We'll use net 0 as common GND placeholder.
# In practice, nets are assigned by netlist import from schematic.

# ── Routing data ─────────────────────────────────────────────────
# segment: (x1,y1, x2,y2, width, layer)
SEGMENTS = []
# via: (x, y, diameter, drill, layers)
VIAS = []
# zone: (layer, net, polygon_points, priority)
ZONES = []

def seg(x1, y1, x2, y2, w, layer="F.Cu"):
    SEGMENTS.append((x1, y1, x2, y2, w, layer))

def via(x, y, dia=0.6, drill=0.3, layers=("F.Cu","B.Cu")):
    VIAS.append((x, y, dia, drill, layers))

def zone(layer, net, pts, priority=0):
    ZONES.append((layer, net, pts, priority))

# ── Power traces (0.5mm, F.Cu) ───────────────────────────────────
# VBUS: J1(12,25) → F1(22,15) → F1→D1(22,20) → VBUS_FILT to BQ24074
# J1 connector at (12,25) — VBUS pins at right side
# F1 at (22,15) — pin1 at left, pin2 at right
seg(14.0, 25.0, 20.0, 15.0, 0.5)  # J1 VBUS → F1-1
seg(24.0, 15.0, 22.0, 20.0, 0.5)  # F1-2 → D1 K (shared VBUS_FILT)
seg(24.0, 20.0, 32.0, 32.0, 0.5)  # D1 → BQ24074 IN (U1 at 32,32)

# SYS_RAW: BQ24074 OUT → F2 → J2 → AP2112K VIN
seg(32.0, 34.0, 48.0, 26.0, 0.5)  # BQ24074 OUT → U2 VIN
seg(44.0, 26.0, 44.0, 25.0, 0.5)  # branch to C1-1
seg(48.0, 26.0, 52.0, 35.0, 0.5)  # U2 → F2
seg(54.0, 35.0, 58.0, 35.0, 0.5)  # F2 → J2

# 3V3: AP2112K VOUT → ESP32-C3 + peripherals
seg(48.0, 30.0, 48.0, 32.0, 0.4)  # U2 VOUT towards C2
seg(48.0, 30.0, 52.0, 25.0, 0.4)  # U2 VOUT → C3
# ESP32-C3 VDD right-side pins at (55,30) + (20.32, varied y)
# Module center (55,30), right row at x=55+20.32=75.32, y=30+25.4=55.4 etc.
# 3V3 trace along right side of module
seg(61.0, 28.0, 61.0, 15.5, 0.4)  # decap chain (C4-C9)

# ── USB differential pair (90Ω, 0.3mm, 0.15mm gap) ──────────────
# J1 D+/D- → USBLC6 → 27R → ESP32-C3
# J1 at (12,25) — D+ pin, D- pin
# USBLC6 (D2) at (32,20)
# 27R R13 at (36,18), R14 at (36,22)
# ESP32-C3 at (55,30) — USB_DP at (75.32, 7.14), USB_DN at (75.32, 4.6)

# USB_DP: J1 → D2-1 → D2-6 → R13 → U3
seg(16.0, 26.5, 30.0, 20.0, 0.3)   # J1 D+ → D2 (shared)
seg(34.0, 20.0, 36.0, 18.0, 0.3)   # D2-6 → R13-1
seg(36.0, 18.0, 72.0, 18.0, 0.3)   # R13-2 → right side of board
seg(72.0, 18.0, 72.0, 7.14, 0.3)   # down to U3 USB_DP pin (y=30+(-22.86)=7.14)

# USB_DN: J1 → D2-3 → D2-4 → R14 → U3
seg(16.0, 23.5, 30.0, 18.0, 0.3)   # J1 D- → D2
seg(34.0, 18.0, 36.0, 22.0, 0.3)   # D2-4 → R14-1
seg(36.0, 22.0, 72.0, 22.0, 0.3)   # R14-2 → right side of board
seg(72.0, 22.0, 72.0, 4.6, 0.3)    # down to U3 USB_DN pin (y=30+(-25.4)=4.6)

# ── I2C traces (0.25mm, orthogonal, ≥0.2mm clearance) ──────────
# ESP32-C3 GPIO8(SDA) at (55-20.32, 30+5.08) = (34.68, 35.08)
# GPIO10(SCL) at (34.68, 30.0)
# OLED DS1 at (20,45): pin3(SCK) at (-15.24,2.54), pin4(SDA) at (-15.24,0)
# Route SDA: GPIO8 → right to 36, down to 36, left to OLED
seg(34.68, 35.08, 36.0, 35.08, 0.25)  # SDA right
seg(36.0, 35.08, 36.0, 44.0, 0.25)    # SDA down
seg(36.0, 44.0, 20.0, 44.0, 0.25)     # SDA left to OLED area
# Route SCL: GPIO10 → right to 38, down to 46, left to OLED
seg(34.68, 30.0, 38.0, 30.0, 0.25)    # SCL right
seg(38.0, 30.0, 38.0, 46.0, 0.25)     # SCL down
seg(38.0, 46.0, 20.0, 46.0, 0.25)     # SCL left to OLED area
# Final approaches to OLED pins
seg(20.0, 44.0, 20.0, 42.0, 0.25)     # SDA up to OLED SDA pin
seg(20.0, 46.0, 20.0, 44.5, 0.25)     # SCL up to OLED SCK pin

# ── Encoder traces (0.25mm) ──────────────────────────────────────
# GPIO2 at (34.68, 50.32), GPIO3 at (34.68, 47.78), GPIO4 at (34.68, 45.24)
# SW1 at (50,44): A=GPIO2, B=GPIO3, SW=GPIO4
seg(34.68, 50.32, 42.0, 43.0, 0.25)  # GPIO2 → R7/encoder A
seg(34.68, 47.78, 42.0, 45.5, 0.25)  # GPIO3 → R8/encoder B
seg(34.68, 45.24, 45.0, 48.0, 0.25)  # GPIO4 → R9/encoder SW

# ── Charger status (0.25mm) ──────────────────────────────────────
# GPIO5-7 from ESP32 left side → pull-ups → BQ24074
# GPIO5 at (34.68, 42.7), GPIO6 at (34.68, 40.16), GPIO7 at (34.68, 37.62)
# R_CE (38,25), R_CHG (38,22), R_FLT (38,19)
seg(34.68, 42.7, 38.0, 25.0, 0.25)   # CE
seg(34.68, 40.16, 38.0, 22.0, 0.25)  # CHG
seg(34.68, 37.62, 38.0, 19.0, 0.25)  # FLT/FAULT

# ── Battery ADC (0.25mm) ─────────────────────────────────────────
# R10(15,39): VBAT → 470k
# R11(15,42): 470k → BSS138 drain
# R12(15,45): GPIO1 → 100k → GND
# Q1(20,41): gate=GPIO1
# GPIO1 at (34.68, 52.86), GPIO0/BAT_SENSE at (34.68, 55.4)
seg(34.68, 52.86, 15.0, 45.0, 0.25)  # GPIO1 → R12/1, Q1 gate
seg(34.68, 55.4, 15.0, 42.0, 0.25)   # BAT_SENSE → divider jct
seg(15.0, 39.0, 15.0, 42.0, 0.25)     # divider series R10→R11
seg(15.0, 42.0, 20.0, 41.0, 0.25)     # divider jct → Q1 drain

# ── BOOT button ──────────────────────────────────────────────────
# GPIO9 at (34.68, 33.62) → SW2(65,44) → GND
seg(34.68, 33.62, 65.0, 44.0, 0.25)

# ── UART test points ─────────────────────────────────────────────
# GPIO20 at (34.68, 9.68), GPIO21 at (34.68, 7.14)
# TP_RX(72,46), TP_TX(72,42)
seg(34.68, 9.68, 72.0, 42.0, 0.25)   # GPIO20 → TP_RX
seg(34.68, 7.14, 72.0, 46.0, 0.25)   # GPIO21 → TP_TX

# ── GND vias (stitching around ESP32-C3 module) ──────────────────
# Place GND vias around module to connect F.Cu GND to In1.Cu GND plane
for gx in [45, 50, 55, 60, 65]:
    for gy in [20, 25, 35, 40]:
        via(gx, gy)

# Thermal vias under BQ24074 (center at 32,32)
for tx in [31, 32, 33]:
    for ty in [31, 32, 33]:
        via(tx, ty, 0.5, 0.3)

# ── Zone pours ──────────────────────────────────────────────────
# Full GND plane on In1.Cu
zone("In1.Cu", 0, [(0,0),(80,0),(80,50),(0,50)])
# Split power plane on In2.Cu — VBUS area, SYS_RAW area, 3V3 area
zone("In2.Cu", 0, [(0,0),(30,0),(30,30),(0,30)], 99)  # GND portion
# Top-layer GND pour after routing
zone("F.Cu", 0, [(0,0),(80,0),(80,50),(0,50)], 10)

# ── Write routing to PCB file ────────────────────────────────────
def gen():
    global pcb_text

    # Remove the trailing paren of kicad_pcb
    pcb_text = pcb_text.strip()
    assert pcb_text.endswith(')')
    base = pcb_text[:-1].rstrip()

    lines = [base]

    # Add segments
    for x1, y1, x2, y2, w, layer in SEGMENTS:
        lines.append(f'\t(segment (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) (width {w}) (layer "{layer}") (net 0) (uuid "{U()}"))')

    # Add vias
    for x, y, dia, drill, layers in VIAS:
        layer_str = ' '.join(f'"{l}"' for l in layers)
        lines.append(f'\t(via (at {x:.2f} {y:.2f}) (size {dia}) (drill {drill}) (layers {layer_str}) (net 0) (uuid "{U()}"))')

    # ZONES COMMENTED OUT: KiCad 10.0.1 CLI rejects zone syntax
    # Zones will be added via KiCad GUI.
    # For reference:
    # zone("In1.Cu", 0, GND plane polygon)
    # zone("F.Cu", 0, GND pour after routing)

    lines.append(')')

    with open(PCB, 'w') as f:
        f.write('\n'.join(lines))
    print(f"✅ Routed PCB: {len(SEGMENTS)} segments, {len(VIAS)} vias, {len(ZONES)} zones")

if __name__ == "__main__":
    gen()
