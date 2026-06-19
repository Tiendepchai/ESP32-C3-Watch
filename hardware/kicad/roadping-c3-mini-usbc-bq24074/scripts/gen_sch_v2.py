#!/usr/bin/env python3
"""RoadPing C3 Mini KiCad 10 schematic generator v2 — CORRECT CONNECTIONS.
   Pin at-coordinates ARE the connection tips. Wire extends AWAY from body."""
import os, sys, uuid as _u
from datetime import date

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_sch")
U = lambda: str(_u.uuid4())
WL = 2.54  # wire stub length

# ── Pin database ────────────────────────────────────────────────
# (at_x, at_y, angle) — absolute offset from symbol origin to connection tip.
# Angled pins: angle 0=extends right, 180=extends left, 90=extends down, 270=extends up.
# Wire direction AWAY from body (opposite to pin extension):
#   angle 0:   tip at (at_x, at_y). Pin → right. Wire → LEFT from tip.
#   angle 180: tip at (at_x, at_y). Pin → left. Wire → RIGHT from tip.
#   angle 90:  tip at (at_x, at_y). Pin → down. Wire → UP from tip.
#   angle 270: tip at (at_x, at_y). Pin → up. Wire → DOWN from tip.

P = {}

def pin_set(name, defs):
    P[name] = {n: (ax, ay, a) for n, ax, ay, a in defs}

pin_set('ESP32', [
    ('0',-20.32,25.4,0),('1',-20.32,22.86,0),('2',-20.32,20.32,0),
    ('3',-20.32,17.78,0),('4',-20.32,15.24,0),('5',-20.32,12.7,0),
    ('6',-20.32,10.16,0),('7',-20.32,7.62,0),('8',-20.32,5.08,0),
    ('9',-20.32,2.54,0),('10',-20.32,0,0),
    ('18',-20.32,-15.24,0),('19',-20.32,-17.78,0),
    ('20',-20.32,-20.32,0),('21',-20.32,-22.86,0),
    ('EN',-20.32,-27.94,0),
    ('VDD_1',20.32,25.4,180),('VDD3P3',20.32,22.86,180),
    ('VDD3P3_RTC',20.32,20.32,180),('VDD3P3_CPU',20.32,17.78,180),
    ('VDD3P3_USB',20.32,15.24,180),('VDD3P3_RTC2',20.32,12.7,180),
    ('GND_1',20.32,-5.08,180),('GND_2',20.32,-7.62,180),('GND_3',20.32,-10.16,180),
    ('USB_DP',20.32,-22.86,180),('USB_DN',20.32,-25.4,180),
])
pin_set('AP2112', [
    ('1',-10.16,5.08,0),('2',-10.16,-5.08,0),('3',-10.16,0,0),
    ('4',10.16,-5.08,180),('5',10.16,5.08,180),
])
pin_set('OLED', [
    ('1',15.24,7.62,180),('2',15.24,-5.08,180),
    ('3',-15.24,2.54,0),('4',-15.24,0,0),
])
pin_set('PEC11R', [
    ('1',-10.16,2.54,0),('2',10.16,2.54,180),
    ('A',10.16,-2.54,180),('B',10.16,-5.08,180),
    ('C',-10.16,-2.54,0),('S1',-10.16,-5.08,0),('S2',-10.16,-7.62,0),
])
pin_set('R', [('1',-3.81,0,0),('2',3.81,0,180)])
pin_set('C', [('1',-3.81,0,0),('2',3.81,0,180)])
pin_set('Fuse', [('1',-3.81,0,0),('2',3.81,0,180)])
pin_set('D_TVS', [('1',-3.81,0,0),('2',3.81,0,180)])
pin_set('Conn01x02', [('1',-5.08,1.27,0),('2',-5.08,-1.27,0)])
pin_set('TestPoint', [('1',-3.81,0,0)])
pin_set('BQ24074', [
    ('1',12.7,-2.54,180),('2',12.7,2.54,180),
    ('4',-12.7,5.08,0),('5',-12.7,-2.54,0),('6',-12.7,0,0),
    ('7',12.7,-7.62,180),('8',0,-15.24,90),('9',12.7,-10.16,180),
    ('10',12.7,10.16,180),('12',-12.7,-7.62,0),('13',0,15.24,270),
    ('14',-12.7,2.54,0),('15',-12.7,10.16,0),('16',-12.7,-10.16,0),
])
pin_set('USBLC6', [
    ('1',-5.08,0,0),('2',0,-7.62,90),('3',-5.08,-2.54,0),
    ('4',5.08,-2.54,180),('5',0,5.08,270),('6',5.08,0,180),
])
pin_set('BSS138', [
    ('1',-5.08,0,0),('2',2.54,-5.08,90),('3',2.54,5.08,270),
])
pin_set('USBC', [
    ('A4',15.24,25.4,180),('A9',15.24,25.4,180),('B4',15.24,25.4,180),('B9',15.24,25.4,180),
    ('A6',15.24,7.62,180),('A7',15.24,12.7,180),('B6',15.24,5.08,180),('B7',15.24,10.16,180),
    ('A5',15.24,20.32,180),('B5',15.24,17.78,180),
    ('A1',0,-40.64,90),('A12',0,-40.64,90),('B1',0,-40.64,90),('B12',0,-40.64,90),
    ('SH',-7.62,-40.64,90),
])

# Pins to skip (duplicate/NC in symbol)
SKIP_PINS = {'AP2112': ['4'], 'BQ24074': ['3', '11', '17']}

def should_skip(ref, pin, pl):
    return pl in SKIP_PINS and pin in SKIP_PINS[pl]

def abs_tip(sx, sy, pin_def):
    """Absolute pin tip coordinate (symbol origin + at offset)."""
    ax, ay, _ = pin_def
    return (sx + ax, sy + ay)

def wire_coords(sx, sy, pin_def):
    """Return (lx, ly, tx, ty) where:
       lx,ly = label coordinate (wire free end, snapped away from body)
       tx,ty = pin tip coordinate (wire start, at the pin connection point)"""
    ax, ay, ang = pin_def
    tx, ty = sx + ax, sy + ay  # absolute tip (at coordinate)
    if ang == 0:     # pin extends right from tip → wire goes left
        return (tx - WL, ty, tx, ty)
    elif ang == 180: # pin extends left from tip → wire goes right
        return (tx + WL, ty, tx, ty)
    elif ang == 90:  # pin extends down from tip → wire goes up
        return (tx, ty - WL, tx, ty)
    elif ang == 270: # pin extends up from tip → wire goes down
        return (tx, ty + WL, tx, ty)

# ── Component database ───────────────────────────────────────────
# (lib_id, ref, value, x, y, footprint, datasheet, pin_lib)
C = []

def add(lib_id, ref, val, x, y, fp="", ds="", pin_lib=""):
    C.append((lib_id, ref, val, x, y, fp, ds, pin_lib or ref))

# USB-C
add("Connector_USB:USB_C_Receptacle_USBIF", "J1", "TYPE-C-31-M-12",
    50.8, 101.6, "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12", "", "USBC")
add("roadping-c3:Fuse", "F1", "PTC 1.1A", 76.2, 101.6, "", "", "Fuse")
add("roadping-c3:D_TVS", "D1", "PESD5V0S1BA", 76.2, 114.3, "", "", "D_TVS")
add("roadping-c3:R", "R1", "5.1k", 96.52, 109.22, "", "", "R")
add("roadping-c3:R", "R2", "5.1k", 96.52, 116.84, "", "", "R")
add("Power_Protection:USBLC6-2SC6", "D2", "USBLC6-2SC6",
    132.08, 101.6, "Package_TO_SOT_SMD:SOT-23-6", "", "USBLC6")
add("roadping-c3:R", "R13", "27", 139.7, 93.98, "", "", "R")
add("roadping-c3:R", "R14", "27", 139.7, 101.6, "", "", "R")

# BQ24074
add("Battery_Management:BQ24074_RGT", "U1", "BQ24074RGTR", 50.8, 165.1,
    "Package_DFN_QFN:WQFN-20-1EP_3.5x3.5mm_P0.5mm_EP2.7x2.7mm",
    "https://www.ti.com/lit/ds/symlink/bq24074.pdf", "BQ24074")
add("roadping-c3:R", "R3", "1.1k", 30.48, 172.72, "", "", "R")
add("roadping-c3:R", "R4", "2.0k", 30.48, 180.34, "", "", "R")
add("roadping-c3:R", "R5", "10k", 73.66, 175.26, "", "", "R")
add("roadping-c3:R", "R6", "10k", 73.66, 182.88, "", "", "R")
add("roadping-c3:R", "R_CE", "10k", 76.2, 154.94, "", "", "R")
add("roadping-c3:R", "R_CHG", "10k", 76.2, 149.86, "", "", "R")
add("roadping-c3:R", "R_FLT", "10k", 76.2, 144.78, "", "", "R")

# AP2112K + LiPo
add("roadping-c3:AP2112K-3.3", "U2", "AP2112K-3.3", 162.56, 165.1, "", "", "AP2112")
add("roadping-c3:C", "C1", "1uF", 152.4, 175.26, "", "", "C")
add("roadping-c3:C", "C2", "1uF", 152.4, 182.88, "", "", "C")
add("roadping-c3:C", "C3", "10uF", 175.26, 175.26, "", "", "C")
add("roadping-c3:Fuse", "F2", "PTC 1.1A", 175.26, 162.56, "", "", "Fuse")
add("roadping-c3:Conn_01x02", "J2", "JST-PH LiPo", 185.42, 165.1,
    "Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical", "", "Conn01x02")

# ESP32-C3
MX, MY = 228.6, 101.6
add("roadping-c3:ESP32-C3-MINI-1-N4", "U3", "ESP32-C3-MINI-1-N4", MX, MY, "", "", "ESP32")
decap_y = [MY + 25.4, MY + 22.86, MY + 20.32, MY + 17.78, MY + 15.24, MY + 12.7]
for i, dy in enumerate(decap_y):
    add("roadping-c3:C", f"C{4+i}", "100nF", MX + 24.0, dy, "", "", "C")

# Peripherals
add("roadping-c3:OLED_128X64_1.3_I2C", "DS1", "SH1106 OLED", 88.9, 259.08, "", "", "OLED")
add("roadping-c3:PEC11R-4215F-S0024", "SW1", "PEC11R", 157.48, 259.08, "", "", "PEC11R")
add("roadping-c3:R", "R7", "10k", 147.32, 256.54, "", "", "R")
add("roadping-c3:R", "R8", "10k", 147.32, 251.46, "", "", "R")
add("roadping-c3:R", "R9", "10k", 147.32, 246.38, "", "", "R")
add("Transistor_FET:BSS138", "Q1", "BSS138", 88.9, 284.48,
    "Package_TO_SOT_SMD:SOT-23", "", "BSS138")
add("roadping-c3:R", "R10", "470k", 78.74, 281.94, "", "", "R")
add("roadping-c3:R", "R11", "470k", 78.74, 289.56, "", "", "R")
add("roadping-c3:R", "R12", "100k", 78.74, 297.18, "", "", "R")
add("roadping-c3:C", "C9", "100nF", 104.14, 281.94, "", "", "C")
add("roadping-c3:Conn_01x02", "SW2", "BOOT", 205.74, 281.94, "", "", "Conn01x02")
add("roadping-c3:TestPoint", "TP_TX", "UART_TX", 274.32, 254.0, "", "", "TestPoint")
add("roadping-c3:TestPoint", "TP_RX", "UART_RX", 274.32, 246.38, "", "", "TestPoint")

# ── Connectivity matrix ──────────────────────────────────────────
N = []
def n(ref, pin, net): N.append((ref, str(pin), net))

n("J1","A4","VBUS"); n("J1","A9","VBUS"); n("J1","B4","VBUS"); n("J1","B9","VBUS")
n("J1","A6","USB_DP"); n("J1","B6","USB_DP"); n("J1","A7","USB_DN"); n("J1","B7","USB_DN")
n("J1","A5","CC1"); n("J1","B5","CC2")
n("J1","A1","GND"); n("J1","A12","GND"); n("J1","B1","GND"); n("J1","B12","GND")
n("J1","SH","GND")
n("F1","1","VBUS"); n("F1","2","VBUS_FILT"); n("D1","1","GND"); n("D1","2","VBUS_FILT")
n("R1","1","CC1"); n("R1","2","GND"); n("R2","1","CC2"); n("R2","2","GND")
n("D2","1","USB_DP"); n("D2","6","USB_DP_ESD")
n("D2","3","USB_DN"); n("D2","4","USB_DN_ESD")
n("D2","2","GND"); n("D2","5","VBUS")
n("R13","1","USB_DP_ESD"); n("R13","2","USB_DP_C3")
n("R14","1","USB_DN_ESD"); n("R14","2","USB_DN_C3")

n("U1","13","VBUS"); n("U1","10","SYS_RAW"); n("U1","2","VBAT")
n("U1","4","CE_CHG"); n("U1","9","CHG_STAT"); n("U1","7","FLT_STAT")
n("U1","12","ILIM"); n("U1","16","ISET")
n("U1","1","TS"); n("U1","14","TMR"); n("U1","15","ITERM")
n("U1","5","EN2"); n("U1","6","EN1"); n("U1","8","GND")
n("R3","1","ISET"); n("R3","2","GND"); n("R4","1","ILIM"); n("R4","2","GND")
n("R5","1","3V3"); n("R5","2","TS"); n("R6","1","TS"); n("R6","2","GND")
n("R_CE","1","3V3"); n("R_CE","2","CE_CHG")
n("R_CHG","1","3V3"); n("R_CHG","2","CHG_STAT")
n("R_FLT","1","3V3"); n("R_FLT","2","FLT_STAT")

n("U2","1","SYS_RAW"); n("U2","2","GND"); n("U2","3","SYS_RAW"); n("U2","5","3V3")
n("C1","1","SYS_RAW"); n("C1","2","GND"); n("C2","1","3V3"); n("C2","2","GND")
n("C3","1","3V3"); n("C3","2","GND")
n("F2","1","VBAT"); n("F2","2","VBAT_FILT"); n("J2","1","VBAT_FILT"); n("J2","2","GND")

for pin in ["VDD_1","VDD3P3","VDD3P3_RTC","VDD3P3_CPU","VDD3P3_USB","VDD3P3_RTC2"]:
    n("U3", pin, "3V3")
n("U3","GND_1","GND"); n("U3","GND_2","GND"); n("U3","GND_3","GND")
n("U3","0","BAT_SENSE"); n("U3","1","GPIO1")
n("U3","2","GPIO2"); n("U3","3","GPIO3"); n("U3","4","GPIO4")
n("U3","5","CE_CHG"); n("U3","6","CHG_STAT"); n("U3","7","FLT_STAT")
n("U3","8","I2C_SDA"); n("U3","10","I2C_SCL"); n("U3","9","GPIO9")
n("U3","18","USB_DP_C3"); n("U3","19","USB_DN_C3")
n("U3","20","GPIO20"); n("U3","21","GPIO21"); n("U3","EN","CHIP_EN")

for i, dy in enumerate(decap_y):
    n(f"C{4+i}", "1", "3V3"); n(f"C{4+i}", "2", "GND")

n("DS1","1","3V3"); n("DS1","2","GND"); n("DS1","3","I2C_SCL"); n("DS1","4","I2C_SDA")
n("SW1","1","GPIO4"); n("SW1","2","GND")
n("SW1","A","GPIO2"); n("SW1","B","GPIO3"); n("SW1","C","GND")
n("SW1","S1","GND"); n("SW1","S2","GND")
n("R7","1","3V3"); n("R7","2","GPIO2"); n("R8","1","3V3"); n("R8","2","GPIO3")
n("R9","1","3V3"); n("R9","2","GPIO4")
n("Q1","1","GPIO1"); n("Q1","2","GND"); n("Q1","3","BAT_DIV")
n("R10","1","VBAT"); n("R10","2","BAT_SENSE")
n("R11","1","BAT_SENSE"); n("R11","2","BAT_DIV")
n("R12","1","GPIO1"); n("R12","2","GND"); n("C9","1","BAT_SENSE"); n("C9","2","GND")
n("SW2","1","GPIO9"); n("SW2","2","GND")
n("TP_TX","1","GPIO21"); n("TP_RX","1","GPIO20")

# ── Generator ────────────────────────────────────────────────────
def fmt2(v): return f"{v:.2f}"

def gen():
    L = []
    L.append(f'(kicad_sch\n\t(version 20260306)')
    L.append(f'\t(generator "eeschema")\n\t(generator_version "10.0")\n\t(uuid "{U()}")')
    L.append(f'\t(paper "A4")\n\t(title_block\n\t\t(title "RoadPing C3 Mini - USB-C + BQ24074")\n\t\t(date "{date.today().isoformat()}")\n\t\t(rev "schematic-v2-connected")\n\t\t(company "RoadPing")\n\t)')
    # Embedded lib_symbols with roadping-c3: prefix
    L.append('\t(lib_symbols')

    embed_file = os.path.join(PROJ, 'scripts', 'EMBED.txt')
    if os.path.exists(embed_file):
        with open(embed_file) as f:
            L.append(f.read().rstrip())
    L.append('\t)')

    # Position and pin_lib lookups
    pos = {}; pl_of = {}
    for lib_id, ref, val, x, y, fp, ds, pl in C:
        pos[ref] = (x, y); pl_of[ref] = pl

    # Place symbols
    for lib_id, ref, val, x, y, fp, ds, pl in C:
        u = U()
        L.append(f'\n\t(symbol (lib_id "{lib_id}") (at {x:.2f} {y:.2f} 0) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (uuid "{u}")')
        L.append(f'\t\t(property "Reference" "{ref}" (at {x:.2f} {(y-3.81):.2f} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
        L.append(f'\t\t(property "Value" "{val}" (at {x:.2f} {(y+3.81):.2f} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
        if fp: L.append(f'\t\t(property "Footprint" "{fp}" (at {x:.2f} {y:.2f} 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
        if ds: L.append(f'\t\t(property "Datasheet" "{ds}" (at {x:.2f} {y:.2f} 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
        L.append('\t)')

    # Wiring: each pin gets a wire stub + label at the outer end
    gnd_set = set()
    label_coords = {}  # net → (lx, ly) — reuse same label coord for same net

    for ref, pin, netname in N:
        pl = pl_of.get(ref, "")
        if should_skip(ref, pin, pl): continue
        if pl not in P or pin not in P[pl]: continue

        sx, sy = pos[ref]
        pin_def = P[pl][pin]
        lx, ly, tx, ty = wire_coords(sx, sy, pin_def)

        # Round all to 2 decimals
        lx, ly = round(lx, 2), round(ly, 2)
        tx, ty = round(tx, 2), round(ty, 2)

        L.append(f'\t(wire (pts (xy {lx:.2f} {ly:.2f}) (xy {tx:.2f} {ty:.2f})) (stroke (width 0) (type default)) (uuid "{U()}"))')
        # Junction at pin tip to mark the connection
        L.append(f'\t(junction (at {tx:.2f} {ty:.2f}) (uuid "{U()}"))')

        if netname == "GND":
            # GND symbol at the wire's dangling end (where label would go)
            key = (lx, ly)
            if key not in gnd_set:
                gnd_set.add(key)
                L.append(f'\t(symbol (lib_id "roadping-c3:GND") (at {lx:.2f} {ly:.2f} 0) (unit 1) (in_bom no) (on_board no) (uuid "{U()}") (property "Value" "GND" (at {lx:.2f} {(ly+2.54):.2f} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27)))))')
        else:
            is_local = any(s in netname for s in ["_FILT","_ESD","_C3","_DIV","_SENSE"]) or netname in ("CC1","CC2","CHIP_EN")
            if is_local:
                L.append(f'\t(label "{netname}" (at {lx:.2f} {ly:.2f} 0) (effects (font (size 1.27 1.27))) (uuid "{U()}"))')
            else:
                L.append(f'\t(global_label "{netname}" (shape input) (at {lx:.2f} {ly:.2f} 0) (effects (font (size 1.27 1.27))) (uuid "{U()}"))')

    # PWR_FLAG
    for pn, px, py in [("VBUS",96.52,160.02),("SYS_RAW",96.52,165.1),
                        ("3V3",96.52,170.18),("VBAT",96.52,175.26)]:
        L.append(f'\n\t(symbol (lib_id "roadping-c3:PWR_FLAG") (at {px:.2f} {py:.2f} 0) (unit 1) (in_bom no) (on_board no) (uuid "{U()}") (property "Value" "PWR_FLAG" (at {px:.2f} {(py+2.54):.2f} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27)))))')
        L.append(f'\t(wire (pts (xy {px:.2f} {py:.2f}) (xy {px:.2f} {(py-2.54):.2f})) (stroke (width 0) (type default)) (uuid "{U()}"))')
        L.append(f'\t(global_label "{pn}" (shape input) (at {px:.2f} {(py-2.54):.2f} 0) (effects (font (size 1.27 1.27))) (uuid "{U()}"))')

    L.append('\n\t(sheet_instances\n\t\t(path "/" (page "1"))\n\t)\n\t(symbol_instances)\n)')

    with open(SCH, "w") as f:
        f.write('\n'.join(L))
    print(f"✅ Written {SCH} ({len(L)} lines)")

if __name__ == "__main__":
    gen()
