#!/usr/bin/env python3
"""RoadPing C3 Mini KiCad 10 schematic generator — CLEAN"""
import os, uuid as _u, subprocess, json
from datetime import date

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_sch")
U = lambda: str(_u.uuid4())
G = 2.54; grd = lambda v: round(v/G)*G
_ = lambda x: f'\t\t{x}'  # indent level

def gl(name,x,y,o=0): return f'\t(global_label "{name}" (shape input) (at {grd(x)} {grd(y)} {o}) (effects (font (size 1.27 1.27))) (uuid "{U()}"))'
def w(x1,y1,x2,y2): return f'\t(wire (pts (xy {grd(x1)} {grd(y1)}) (xy {grd(x2)} {grd(y2)})) (stroke (width 0) (type default)) (uuid "{U()}"))'
def ll(name,x,y): return f'\t(label "{name}" (at {grd(x)} {grd(y)} 0) (effects (font (size 1.27 1.27))) (uuid "{U()}"))'

def SYM(lib_id, ref, val, x, y, fp="", ds=""):
    x,y = grd(x), grd(y); u = U()
    p = [f'(property "Reference" "{ref}" (at {x} {y-3.81} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))',
         f'(property "Value" "{val}" (at {x} {y+3.81} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))']
    if fp: p.append(f'(property "Footprint" "{fp}" (at {x} {y} 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
    if ds: p.append(f'(property "Datasheet" "{ds}" (at {x} {y} 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))')
    return f'\n\t(symbol (lib_id "{lib_id}") (at {x} {y} 0) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (uuid "{u}")\n\t\t{chr(10).join(p)}\n\t)'

def GND(x,y):
    x,y = grd(x), grd(y); u = U()
    return f'\n\t(symbol (lib_id "roadping-c3:GND") (at {x} {y} 0) (unit 1) (in_bom no) (on_board no) (uuid "{u}") (property "Value" "GND" (at {x} {y+2.54} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27)))))'

def PWR(x,y):
    x,y = grd(x), grd(y); u = U()
    return f'\n\t(symbol (lib_id "roadping-c3:PWR_FLAG") (at {x} {y} 0) (unit 1) (in_bom no) (on_board no) (uuid "{u}") (property "Value" "PWR_FLAG" (at {x} {y+2.54} 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27)))))'

# ── Build ─────────────────────────────────────────────────────
L = []
L.append(f'(kicad_sch\n\t(version 20260306)\n\t(generator "eeschema")\n\t(generator_version "10.0")\n\t(uuid "{U()}")\n\t(paper "A4")\n\t(title_block\n\t\t(title "RoadPing C3 Mini - USB-C + BQ24074")\n\t\t(date "{date.today().isoformat()}")\n\t\t(rev "schematic-draft")\n\t\t(company "RoadPing")\n\t)')

# Embed symbols from a file read
embed_file = os.path.join(PROJ, 'scripts', 'EMBED.txt')
if os.path.exists(embed_file):
    with open(embed_file) as f:
        embed = f.read()
else:
    embed = ''
    sys.stderr.write("WARNING: EMBED.txt not found, creating minimal schematic\n")

L.append(f'\t(lib_symbols\n{embed}\t)')

# 1. USB-C
L.append(SYM("Connector_USB:USB_C_Receptacle_USBIF","J1","TYPE-C-31-M-12",50.8,50.8,fp="Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12"))
L.append(SYM("roadping-c3:Fuse","F1","PTC 1.1A",76.2,50.8))
L.append(SYM("roadping-c3:D_TVS","D1","PESD5V0S1BA",76.2,68.58))
L.append(SYM("roadping-c3:R","R1","5.1k",76.2,78.74))
L.append(SYM("roadping-c3:R","R2","5.1k",76.2,86.36))
L.append(SYM("Interface_Protection:USBLC6-2SC6","D2","USBLC6-2SC6",106.68,53.34))
L.append(w(60.96,50.8,76.2,50.8))
L.append(gl("VBUS",76.2,63.5)); L.append(w(76.2,58.42,76.2,68.58)); L.append(GND(76.2,71.12))
L.append(ll("CC1",66.04,78.74)); L.append(w(66.04,78.74,76.2,78.74)); L.append(GND(76.2,81.28))
L.append(ll("CC2",66.04,86.36)); L.append(w(66.04,86.36,76.2,86.36)); L.append(GND(76.2,88.9))
L.append(ll("USB_DP",66.04,43.18)); L.append(ll("USB_DN",66.04,38.1))
L.append(w(66.04,43.18,96.52,43.18)); L.append(w(66.04,38.1,96.52,38.1))
L.append(gl("USB_DP",115.06,43.18)); L.append(gl("USB_DN",115.06,38.1))

# 2. BQ24074
L.append(SYM("Battery_Management:BQ24074_RGT","U1","BQ24074RGTR",50.8,119.38,fp="Package_DFN_QFN:WQFN-20-1EP_3.5x3.5mm_P0.5mm_EP2.7x2.7mm",ds="https://www.ti.com/lit/ds/symlink/bq24074.pdf"))
L.append(gl("VBUS",40.64,119.38)); L.append(w(40.64,119.38,50.8,119.38))
L.append(SYM("roadping-c3:R","R3","1.1k",33.02,137.16))
L.append(SYM("roadping-c3:R","R4","2.0k",33.02,142.24))
L.append(GND(33.02,133.35)); L.append(GND(33.02,138.43))
L.append(SYM("roadping-c3:R","R5","10k",73.66,129.54))
L.append(SYM("roadping-c3:R","R6","10k",73.66,134.62))
L.append(GND(73.66,137.16))
L.append(SYM("roadping-c3:R","R_CE","10k",76.2,109.22))
L.append(SYM("roadping-c3:R","R_CHG","10k",76.2,104.14))
L.append(SYM("roadping-c3:R","R_FLT","10k",76.2,99.06))
L.append(gl("3V3",83.82,114.3)); L.append(gl("CE_CHG",83.82,109.22))
L.append(gl("CHG_STAT",83.82,104.14)); L.append(gl("FLT_STAT",83.82,99.06))
L.append(gl("SYS_RAW",76.2,119.38)); L.append(gl("VBAT",76.2,127.0))

# 3. AP2112K + LiPo
L.append(SYM("roadping-c3:AP2112K-3.3","U2","AP2112K-3.3",139.7,119.38))
L.append(SYM("roadping-c3:C","C1","1uF",129.54,129.54))
L.append(SYM("roadping-c3:C","C2","1uF",129.54,134.62))
L.append(SYM("roadping-c3:C","C3","10uF",154.94,129.54))
L.append(SYM("roadping-c3:Fuse","F2","PTC 1.1A",157.48,119.38))
L.append(SYM("roadping-c3:Conn_01x02","J2","JST-PH LiPo",170.18,124.46,fp="Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical"))
L.append(gl("SYS_RAW",129.54,119.38)); L.append(gl("3V3",154.94,116.84))
L.append(gl("VBAT",154.94,124.46))
L.append(GND(129.54,127.0)); L.append(GND(129.54,132.08)); L.append(GND(154.94,127.0))
L.append(GND(170.18,121.92))

# 4. ESP32-C3-MINI-1
MX, MY = 248.92, 50.8
L.append(SYM("roadping-c3:ESP32-C3-MINI-1-N4","U3","ESP32-C3-MINI-1-N4",MX,MY))

left_pins = [(0,25.4,"GPIO0"),(1,22.86,"GPIO1"),(2,20.32,"GPIO2"),(3,17.78,"GPIO3"),(4,15.24,"GPIO4"),
    (5,12.7,"GPIO5"),(6,10.16,"GPIO6"),(7,7.62,"GPIO7"),(8,5.08,"I2C_SDA"),(9,2.54,"GPIO9"),(10,0,"I2C_SCL"),
    (18,-15.24,"GPIO18"),(19,-17.78,"GPIO19"),(20,-20.32,"GPIO20"),(21,-22.86,"GPIO21")]
for pn,py,ln in left_pins:
    L.append(w(MX-22.86, MY+py, MX-20.32, MY+py))
    L.append(gl(ln, MX-22.86, MY+py))
for py in [25.4,22.86,20.32,17.78,15.24,12.7]:
    L.append(w(MX+20.32, MY+py, MX+22.86, MY+py))
    L.append(gl("3V3", MX+22.86, MY+py))
for py in [-5.08,-7.62,-10.16]:
    L.append(w(MX+20.32, MY+py, MX+22.86, MY+py))
    L.append(GND(MX+25.4, MY+py))
L.append(gl("CE_CHG",MX-30.48,MY+15.24))
L.append(gl("CHG_STAT",MX-30.48,MY+12.7))
L.append(gl("FLT_STAT",MX-30.48,MY+10.16))
L.append(gl("I2C_SDA",MX-30.48,MY+7.62))
L.append(gl("I2C_SCL",MX-30.48,MY+5.08))
L.append(gl("GPIO2",MX-22.86,MY+22.86))
L.append(gl("GPIO3",MX-22.86,MY+20.32))
L.append(gl("GPIO4",MX-22.86,MY+17.78))

# 5. Peripherals
L.append(SYM("roadping-c3:OLED_128X64_1.3_I2C","DS1","SH1106 OLED",88.9,243.84))
L.append(gl("3V3",104.14,243.84)); L.append(GND(104.14,238.76))
L.append(gl("I2C_SCL",78.74,243.84)); L.append(gl("I2C_SDA",78.74,238.76))
L.append(SYM("roadping-c3:PEC11R-4215F-S0024","SW1","PEC11R",154.94,243.84))
L.append(SYM("roadping-c3:R","R7","10k",144.78,241.3))
L.append(SYM("roadping-c3:R","R8","10k",144.78,236.22))
L.append(SYM("roadping-c3:R","R9","10k",144.78,231.14))
L.append(gl("3V3",137.16,243.84))
L.append(gl("GPIO2",170.18,243.84)); L.append(gl("GPIO3",170.18,238.76)); L.append(gl("GPIO4",170.18,233.68))
L.append(SYM("Transistor_FET:BSS138","Q1","BSS138",88.9,269.24,fp="Package_TO_SOT_SMD:SOT-23"))
L.append(SYM("roadping-c3:R","R10","470k",78.74,266.7))
L.append(SYM("roadping-c3:R","R11","470k",78.74,271.78))
L.append(SYM("roadping-c3:R","R12","100k",78.74,276.86))
L.append(SYM("roadping-c3:C","C9","100nF",104.14,269.24))
L.append(gl("VBAT",73.66,266.7)); L.append(gl("GPIO0",109.22,266.7)); L.append(gl("GPIO1",109.22,271.78))
L.append(GND(104.14,264.18)); L.append(GND(78.74,274.32)); L.append(GND(78.74,279.4))

L.append(SYM("roadping-c3:Conn_01x02","SW2","BOOT",210.82,269.24))
L.append(gl("GPIO9",205.74,269.24)); L.append(GND(210.82,264.18))

L.append(SYM("roadping-c3:TestPoint","TP_TX","UART_TX",279.4,248.92))
L.append(SYM("roadping-c3:TestPoint","TP_RX","UART_RX",279.4,241.3))
L.append(gl("GPIO20",274.32,248.92)); L.append(gl("GPIO21",274.32,241.3))

L.append('\n\t(sheet_instances\n\t\t(path "/" (page "1"))\n\t)\n\t(symbol_instances)\n)')

with open(SCH,"w") as f: f.write('\n'.join(L))
print(f"✅ Written {SCH} — {len(L)} lines")
