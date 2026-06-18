#!/usr/bin/env python3
"""RoadPing C3 Mini — PCB placement (KiCad 10)."""
import os, uuid as _u

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")
U = lambda: str(_u.uuid4())
BW, BH = 80.0, 50.0

F = []
def fp(path, ref, val, x, y, rot=0):
    F.append((path, ref, val, x, y, rot))

fp("Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12","J1","TYPE-C-31-M-12",12.0,25.0,90)
fp("Fuse:Fuse_1206_3216Metric","F1","PTC 1.1A",22.0,15.0)
fp("Diode_SMD:D_SOD-323","D1","PESD5V0S1BA",22.0,20.0)
fp("Resistor_SMD:R_0603_1608Metric","R1","5.1k",28.0,15.0,90)
fp("Resistor_SMD:R_0603_1608Metric","R2","5.1k",28.0,20.0,90)
fp("Package_TO_SOT_SMD:SOT-23-6","D2","USBLC6-2SC6",32.0,20.0)
fp("Resistor_SMD:R_0603_1608Metric","R13","27",36.0,18.0,90)
fp("Resistor_SMD:R_0603_1608Metric","R14","27",36.0,22.0,90)
fp("Package_DFN_QFN:WQFN-20-1EP_3.5x3.5mm_P0.5mm_EP2.7x2.7mm","U1","BQ24074RGTR",32.0,32.0)
fp("Resistor_SMD:R_0603_1608Metric","R3","1.1k",26.0,30.0)
fp("Resistor_SMD:R_0603_1608Metric","R4","2.0k",26.0,35.0)
fp("Resistor_SMD:R_0603_1608Metric","R5","10k",38.0,30.0)
fp("Resistor_SMD:R_0603_1608Metric","R6","10k",38.0,35.0)
fp("Resistor_SMD:R_0603_1608Metric","R_CE","10k",38.0,25.0)
fp("Resistor_SMD:R_0603_1608Metric","R_CHG","10k",38.0,22.0)
fp("Resistor_SMD:R_0603_1608Metric","R_FLT","10k",38.0,19.0)
fp("Package_TO_SOT_SMD:SOT-23-5","U2","AP2112K-3.3",48.0,28.0)
fp("Capacitor_SMD:C_0603_1608Metric","C1","1uF",44.0,25.0)
fp("Capacitor_SMD:C_0603_1608Metric","C2","1uF",44.0,32.0)
fp("Capacitor_SMD:C_0603_1608Metric","C3","10uF",52.0,25.0)
fp("Fuse:Fuse_1206_3216Metric","F2","PTC 1.1A",52.0,35.0)
fp("Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical","J2","LiPo",58.0,35.0)
fp("roadping-c3:MODULE_ESP32-C3-MINI-1-N4","U3","ESP32-C3-MINI-1-N4",55.0,30.0)
for i, dy in enumerate([28.0,25.5,23.0,20.5,18.0,15.5]):
    fp("Capacitor_SMD:C_0603_1608Metric",f"C{4+i}","100nF",61.0,dy)
fp("roadping-c3:OLED_4PIN_VDD_GND_SCK_SDA_2.54MM","DS1","SH1106 OLED",20.0,45.0)
fp("roadping-c3:XDCR_PEC11R-4215F-S0024","SW1","PEC11R",50.0,44.0)
fp("Resistor_SMD:R_0603_1608Metric","R7","10k",42.0,43.0)
fp("Resistor_SMD:R_0603_1608Metric","R8","10k",42.0,45.5)
fp("Resistor_SMD:R_0603_1608Metric","R9","10k",45.0,48.0)
fp("Package_TO_SOT_SMD:SOT-23","Q1","BSS138",20.0,41.0)
fp("Resistor_SMD:R_0603_1608Metric","R10","470k",15.0,39.0)
fp("Resistor_SMD:R_0603_1608Metric","R11","470k",15.0,42.0)
fp("Resistor_SMD:R_0603_1608Metric","R12","100k",15.0,45.0)
fp("Capacitor_SMD:C_0603_1608Metric","C9","100nF",22.0,44.0)
fp("Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical","SW2","BOOT",65.0,44.0,90)
fp("roadping-c3:TP_Round_1.0mm","TP_TX","UART_TX",72.0,42.0)
fp("roadping-c3:TP_Round_1.0mm","TP_RX","UART_RX",72.0,46.0)
for mx, my in [(5,5),(5,45),(75,5),(75,45)]:
    fp("MountingHole:MountingHole_2.2mm_M2",f"MH_{int(mx)}_{int(my)}","M2",mx,my)

def gen():
    L = []; a = lambda s: L.append(s)
    a('(kicad_pcb'); a('\t(version 20260206)'); a('\t(generator "pcbnew")')
    a('\t(generator_version "10.0")')
    a('\t(general'); a('\t\t(thickness 1.6)'); a('\t)')
    a('\t(paper "A4")')
    a('\t(title_block'); a('\t\t(title "RoadPing C3 Mini - USB-C + BQ24074")')
    a('\t\t(date "2026-06-17")'); a('\t\t(rev "v1")'); a('\t)')
    a('\t(layers')
    a('\t\t(0 "F.Cu" signal)'); a('\t\t(2 "B.Cu" signal)')
    a('\t\t(4 "In1.Cu" signal "In1.Cu(GND)")')
    a('\t\t(6 "In2.Cu" signal "In2.Cu(PWR)")')
    for n, name in [(9,"F.Adhes"),(11,"B.Adhes"),(13,"F.Paste"),(15,"B.Paste"),(5,"F.SilkS"),(7,"B.SilkS"),(1,"F.Mask"),(3,"B.Mask"),(17,"Dwgs.User"),(19,"Cmts.User"),(21,"Eco1.User"),(23,"Eco2.User"),(25,"Edge.Cuts"),(27,"Margin"),(31,"F.CrtYd"),(29,"B.CrtYd"),(35,"F.Fab"),(33,"B.Fab")]:
        a(f'\t\t({n} "{name}" user)')
    a('\t)')
    a('\t(setup'); a('\t\t(stackup')
    for l, t in [("F.Cu","copper"),("In1.Cu","copper"),("In2.Cu","copper"),("B.Cu","copper")]:
        a(f'\t\t\t(layer "{l}" (type "{t}") (thickness 0.035))')
    a('\t\t)'); a('\t)')
    a('')  # blank line
    a('\t(net 0 "")')  # placeholder net for routing segments
    # Edge.Cuts outline (gr_line works around KiCad 10.0.1 fp_line regression)
    a(f'\t(gr_line (start 0 0) (end {BW} 0) (stroke (width 0.05) (type solid)) (layer "Edge.Cuts") (uuid "{U()}"))')
    a(f'\t(gr_line (start {BW} 0) (end {BW} {BH}) (stroke (width 0.05) (type solid)) (layer "Edge.Cuts") (uuid "{U()}"))')
    a(f'\t(gr_line (start {BW} {BH}) (end 0 {BH}) (stroke (width 0.05) (type solid)) (layer "Edge.Cuts") (uuid "{U()}"))')
    a(f'\t(gr_line (start 0 {BH}) (end 0 0) (stroke (width 0.05) (type solid)) (layer "Edge.Cuts") (uuid "{U()}"))')
    for path, ref, val, x, y, rot in F:
        a(f'\n\t(footprint "{path}"')
        a(f'\t\t(layer "F.Cu")'); a(f'\t\t(uuid "{U()}")')
        a(f'\t\t(at {x:.2f} {y:.2f} {rot})')
        a(f'\t\t(descr "{ref} {val}")')
        a('\t\t(attr smd)')
        a(f'\t\t(solder_mask_margin 0.025)')
        a(f'\t\t(property "Reference" "{ref}" (at {x:.2f} {(y-2):.2f} 0) (layer "F.SilkS") (uuid "{U()}") (effects (font (size 1 1) (thickness 0.15))))')
        a(f'\t\t(property "Value" "{val}" (at {x:.2f} {(y+2):.2f} 0) (layer "F.Fab") (uuid "{U()}") (effects (font (size 1 1) (thickness 0.15))))')
        a('\t\t(embedded_fonts no)'); a('\t)')
    a('\n)')
    with open(PCB, 'w') as f: f.write('\n'.join(L))
    print(f"✅ Written {PCB} ({len(L)} lines, {len(F)} footprints)")

if __name__ == "__main__": gen()
