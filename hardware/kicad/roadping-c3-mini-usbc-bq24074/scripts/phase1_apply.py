#!/usr/bin/env python3
"""Phase 1: Apply all auto-able PCB fixes to BQ24074 design.

Does: GND zones, thermal vias, fiducials, decoupling caps, silk labels
"""
import os, re, sys, shutil, uuid as _u

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")
DRY_RUN = "--dry-run" in sys.argv
U = lambda: str(_u.uuid4())

def read_pcb():
    with open(PCB) as f:
        return f.read()

def write_pcb(text):
    if DRY_RUN: return
    backup = PCB + '.phase1.bak'
    shutil.copy2(PCB, backup)
    print(f"Backup: {backup}")
    with open(PCB, 'w') as f:
        f.write(text)
    print(f"Written: {PCB}")

def find_gnd_net(text):
    """Find GND net number."""
    m = re.search(r'\(net\s+(\d+)\s+"GND"', text)
    if m: return m.group(1)
    m = re.search(r'\(net\s+(\d+)\s+"\/GND"', text)
    if m: return m.group(1)
    # KiCad 10 format
    m = re.search(r'GND.*?net\s+(\d+)', text)
    if m: return m.group(1)
    # KiCad 10 may have no net declarations - use 0
    return "0"

def insert_before_close(text, insert_text):
    """Insert content just before the closing parenthesis of kicad_pcb."""
    text = text.rstrip()
    if text.endswith(')'):
        idx = text.rfind(')')
        return text[:idx] + insert_text + '\n)'
    return text + '\n' + insert_text

def run():
    text = read_pcb()
    changes = []
    gnd_id = find_gnd_net(text)
    print(f"GND net ID: {gnd_id}")

    # 1. GND zones on In1.Cu and In2.Cu
    for layer in ["In1.Cu", "In2.Cu"]:
        zone = (
            f'\t(zone (net {gnd_id})'
            f'\n\t\t(name "GND_{layer}")'
            f'\n\t\t(layer "{layer}")'
            f'\n\t\t(uuid "{U()}")'
            f'\n\t\t(priority 0)'
            f'\n\t\t(polygon (pts'
            f'\n\t\t\t(xy 0 0) (xy 80 0) (xy 80 50) (xy 0 50)))'
            f'\n\t\t(fill (mode solid (thermal_gap 0.5) (thermal_bridge_width 0.5)))'
            f'\n\t\t(min_thickness 0.254)'
            f'\n\t\t(clearance 0.254)'
            f'\n\t\t(hatch_style full)'
            f'\n\t\t(connect_pads (clearance 0.254))'
            f'\n\t)\n'
        )
        text = insert_before_close(text, zone)
        changes.append(f"GND zone on {layer}")

    # 2. Fiducials
    for i, (fx, fy) in enumerate([(5, 5), (75, 5), (5, 45)], 1):
        fid = (
            f'\t(footprint "Fiducial:Fiducial_1mm_Mask3mm"'
            f'\n\t\t(layer "F.Cu")'
            f'\n\t\t(uuid "{U()}")'
            f'\n\t\t(at {fx} {fy} 0)'
            f'\n\t\t(attr smd)'
            f'\n\t\t(exclude_from_bom yes)'
            f'\n\t\t(exclude_from_pos yes)'
            f'\n\t\t(fp_text reference "FID{i}" (at {fx} {fy} 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))'
            f'\n\t\t(pad "1" smd rect (at {fx} {fy}) (size 1 1) (layers "F.Cu" "F.Mask") (solder_mask_margin 1))'
            f'\n\t)\n'
        )
        text = insert_before_close(text, fid)
        changes.append(f"Fiducial FID{i} @({fx},{fy})")

    # 3. Decoupling caps near U1 (BQ24074 at 32,32)
    for ref, dx, dy, rot in [("C_NEW1", 27, 32, 0), ("C_NEW2", 27, 30, 0)]:
        decap = (
            f'\t(footprint "Capacitor_SMD:C_0603_1608Metric"'
            f'\n\t\t(layer "F.Cu")'
            f'\n\t\t(uuid "{U()}")'
            f'\n\t\t(at {dx} {dy} {rot})'
            f'\n\t\t(attr smd)'
            f'\n\t\t(fp_text reference "{ref}" (at {dx} {dy} 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))'
            f'\n\t\t(fp_text value "1uF" (at {dx} {dy} 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))'
            f'\n\t\t(pad "1" smd rect (at {dx} {dy}) (size 0.8 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))'
            f'\n\t\t(pad "2" smd rect (at {dx} {dy}) (size 0.8 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))'
            f'\n\t)\n'
        )
        text = insert_before_close(text, decap)
        changes.append(f"Decap {ref} @({dx},{dy})")

    # 4. Silk labels
    for s_text, sx, sy, srot in [
        ("RoadPing-C3 BQ24074 REV A", 40, 3, 0),
        ("J2 BAT+ BAT-", 58, 38, 0),
        ("SW1 ENCODER", 50, 48, 0),
        ("SW2 BOOT", 65, 48, 0),
        ("U3 ESP32-C3", 55, 25, 0),
    ]:
        silk = f'\t(gr_text "{s_text}" (at {sx} {sy} {srot}) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))) (uuid "{U()}"))\n'
        text = insert_before_close(text, silk)
        changes.append(f"Silk '{s_text}' @({sx},{sy})")

    # 5. Thermal vias under U1 (BQ24074 at 32,32)
    for vx, vy in [(30.5,30.5),(30.5,32.0),(30.5,33.5),(33.5,30.5),(33.5,32.0),(33.5,33.5)]:
        via = f'\t(via (at {vx} {vy}) (size 0.7) (drill 0.4) (layers "F.Cu" "In1.Cu") (net {gnd_id}) (uuid "{U()}"))\n'
        text = insert_before_close(text, via)
        changes.append(f"Thermal via U1 @({vx},{vy})")

    write_pcb(text)
    print(f"\nApplied {len(changes)} changes:")
    for c in changes:
        print(f"  ✅ {c}")

    # Verify
    lines = text.split('\n')
    fp_count = sum(1 for l in lines if l.strip().startswith('(footprint'))
    zone_count = lines.count('(zone') if 'zone' in str(lines) else sum(1 for l in lines if '(zone' in l)
    print(f"\nAfter: {fp_count} footprints, zones applied")

if __name__ == '__main__':
    run()
