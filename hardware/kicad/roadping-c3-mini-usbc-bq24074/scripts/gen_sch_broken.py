#!/usr/bin/env python3
"""
RoadPing C3 Mini KiCad 10 schematic generator — PROPER WIRING.
Every global label sits at the end of a short wire stub from each symbol pin.
"""

import os, uuid as _uuid
from datetime import date

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_sch")
U = lambda: str(_uuid.uuid4())
E = lambda s: f'(effects (font (size {s} {s})))'
PT = lambda x,y,o=0: f'(at {x} {y} {o})'
XY = lambda a,b: f'(xy {a} {b})'
G = 2.54  # KiCad grid = 2.54mm (100mil)
grd = lambda v: round(v/G)*G  # snap to grid

def pin(pin_type, name, num, x, y, orient):
    return f'(pin {pin_type} line {PT(x,y,orient)} (length 5.08) (name "{name}" {E(1.016)}) (number "{num}" {E(1.016)}))'

def gl(name, x, y, o=0):
    return f'\t(global_label "{name}" (shape input) {PT(x,y,o)} (effects (font (size 1.27 1.27))) (uuid "{U()}"))'

def ll(name, x, y, o=0):
    return f'\t(label "{name}" {PT(x,y,o)} (effects (font (size 1.27 1.27))) (uuid "{U()}"))'

def w(x1,y1,x2,y2):
    return f'\t(wire (pts {XY(x1,y1)} {XY(x2,y2)}) (stroke (width 0) (type default)) (uuid "{U()}"))'

def sym(lib_id, ref, value, x, y, orient=0, fp="", ds="", desc="", dnp=False):
    u = U()
    props = [
        f'(property "Reference" "{ref}" {PT(x,y-3.81)} (show_name no) (do_not_autoplace no) {E(1.27)})',
        f'(property "Value" "{value}" {PT(x,y+3.81)} (show_name no) (do_not_autoplace no) {E(1.27)})',
    ]
    if fp: props.append(f'(property "Footprint" "{fp}" {PT(x,y)} (hide yes) (show_name no) (do_not_autoplace no) {E(1.27)})')
    if ds: props.append(f'(property "Datasheet" "{ds}" {PT(x,y)} (hide yes) (show_name no) (do_not_autoplace no) {E(1.27)})')
    if desc: props.append(f'(property "Description" "{desc}" {PT(x,y)} (hide yes) (show_name no) (do_not_autoplace no) {E(1.27)})')
    return f'''
	(symbol
		(lib_id "{lib_id}")
		{PT(x,y,orient)}
		(unit 1)
		(exclude_from_sim no)
		(in_bom {"yes" if not dnp else "no"})
		(on_board yes)
		(in_pos_files {"yes" if not dnp else "no"})
		(dnp {"yes" if dnp else "no"})
		(uuid "{u}")
		{chr(10).join(props)}
	)'''

def pwr_sym(part, x, y):
    u = U()
    return f'''
	(symbol
		(lib_id "roadping-c3:{part}")
		{PT(x,y)}
		(unit 1)
		(exclude_from_sim no)
		(in_bom no)
		(on_board no)
		(uuid "{u}")
		(property "Value" "{part}" {PT(x,y+2.54)} (show_name no) (do_not_autoplace no) {E(1.27)})
	)'''

# ── Embedded symbols (unchanged from working version) ─────────

EMBED = r'''
	(symbol "roadping-c3:ESP32-C3-MINI-1-N4" (in_bom yes) (on_board yes)
		(property "Reference" "U" (at 0 31.75 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "ESP32-C3-MINI-1-N4" (at 0 -36.83 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "roadping-c3:MODULE_ESP32-C3-MINI-1-N4" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Datasheet" "https://www.espressif.com/sites/default/files/documentation/ESP32-C3-MINI-1_Datasheet_EN.pdf" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "ESP32-C3-MINI-1-N4_0_0"
			(rectangle (start -15.24 -27.94) (end 15.24 27.94) (stroke (width 0.254) (type default)) (fill (type background)))
			(pin bidirectional line (at -20.32 25.4 0) (length 5.08) (name "GPIO0" (effects (font (size 1.016 1.016)))) (number "0" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 22.86 0) (length 5.08) (name "GPIO1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 20.32 0) (length 5.08) (name "GPIO2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 17.78 0) (length 5.08) (name "GPIO3" (effects (font (size 1.016 1.016)))) (number "3" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 15.24 0) (length 5.08) (name "GPIO4" (effects (font (size 1.016 1.016)))) (number "4" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 12.7 0) (length 5.08) (name "GPIO5" (effects (font (size 1.016 1.016)))) (number "5" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 10.16 0) (length 5.08) (name "GPIO6" (effects (font (size 1.016 1.016)))) (number "6" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 7.62 0) (length 5.08) (name "GPIO7" (effects (font (size 1.016 1.016)))) (number "7" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 5.08 0) (length 5.08) (name "GPIO8" (effects (font (size 1.016 1.016)))) (number "8" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 2.54 0) (length 5.08) (name "GPIO9" (effects (font (size 1.016 1.016)))) (number "9" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 0 0) (length 5.08) (name "GPIO10" (effects (font (size 1.016 1.016)))) (number "10" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 -15.24 0) (length 5.08) (name "GPIO18" (effects (font (size 1.016 1.016)))) (number "18" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 -17.78 0) (length 5.08) (name "GPIO19" (effects (font (size 1.016 1.016)))) (number "19" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 -20.32 0) (length 5.08) (name "GPIO20" (effects (font (size 1.016 1.016)))) (number "20" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -20.32 -22.86 0) (length 5.08) (name "GPIO21" (effects (font (size 1.016 1.016)))) (number "21" (effects (font (size 1.016 1.016)))))
			(pin input line (at -20.32 -27.94 0) (length 5.08) (name "CHIP_EN" (effects (font (size 1.016 1.016)))) (number "EN" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 25.4 180) (length 5.08) (name "VDD" (effects (font (size 1.016 1.016)))) (number "VDD_1" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 22.86 180) (length 5.08) (name "VDD3P3" (effects (font (size 1.016 1.016)))) (number "VDD3P3" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 20.32 180) (length 5.08) (name "VDD3P3_RTC" (effects (font (size 1.016 1.016)))) (number "VDD3P3_RTC" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 17.78 180) (length 5.08) (name "VDD3P3_CPU" (effects (font (size 1.016 1.016)))) (number "VDD3P3_CPU" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 15.24 180) (length 5.08) (name "VDD3P3_USB" (effects (font (size 1.016 1.016)))) (number "VDD3P3_USB" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 12.7 180) (length 5.08) (name "VDD3P3_RTC2" (effects (font (size 1.016 1.016)))) (number "VDD3P3_RTC2" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 -5.08 180) (length 5.08) (name "GND" (effects (font (size 1.016 1.016)))) (number "GND_1" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 -7.62 180) (length 5.08) (name "GND" (effects (font (size 1.016 1.016)))) (number "GND_2" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 20.32 -10.16 180) (length 5.08) (name "GND" (effects (font (size 1.016 1.016)))) (number "GND_3" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at 20.32 -22.86 180) (length 5.08) (name "USB_D+" (effects (font (size 1.016 1.016)))) (number "USB_DP" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at 20.32 -25.4 180) (length 5.08) (name "USB_D-" (effects (font (size 1.016 1.016)))) (number "USB_DN" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:AP2112K-3.3" (in_bom yes) (on_board yes)
		(property "Reference" "U" (at 0 8.89 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "AP2112K-3.3" (at 0 -8.89 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Package_TO_SOT_SMD:SOT-23-5" (at 0 -11.43 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Datasheet" "https://www.diodes.com/datasheet/download/AP2112.pdf" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Description" "AP2112K fixed 3.3V LDO, SOT-23-5: 1=VIN, 2=GND, 3=EN, 4=NC, 5=VOUT" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "AP2112K-3.3_0_1" (rectangle (start -7.62 6.35) (end 7.62 -6.35) (stroke (width 0.254) (type default)) (fill (type background))))
		(symbol "AP2112K-3.3_1_1"
			(pin power_in line (at -10.16 5.08 0) (length 2.54) (name "VIN" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at -10.16 -5.08 0) (length 2.54) (name "GND" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
			(pin input line (at -10.16 0 0) (length 2.54) (name "EN" (effects (font (size 1.016 1.016)))) (number "3" (effects (font (size 1.016 1.016)))))
			(pin no_connect line (at 10.16 -5.08 180) (length 2.54) (name "NC" (effects (font (size 1.016 1.016)))) (number "4" (effects (font (size 1.016 1.016)))))
			(pin power_out line (at 10.16 5.08 180) (length 2.54) (name "VOUT" (effects (font (size 1.016 1.016)))) (number "5" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:OLED_128X64_1.3_I2C" (in_bom yes) (on_board yes)
		(property "Reference" "DS" (at -10.16 10.922 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "OLED_128X64_1.3_I2C" (at -10.16 -10.16 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "roadping-c3:OLED_4PIN_VDD_GND_SCK_SDA_2.54MM" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Datasheet" "https://cdn-shop.adafruit.com/datasheets/SH1106_datasheet.pdf" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "OLED_128X64_1.3_I2C_0_0"
			(rectangle (start -10.16 -7.62) (end 10.16 10.16) (stroke (width 0.254) (type default)) (fill (type background)))
			(pin power_in line (at 15.24 7.62 180) (length 5.08) (name "VDD" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin power_in line (at 15.24 -5.08 180) (length 5.08) (name "GND" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
			(pin input clock (at -15.24 2.54 0) (length 5.08) (name "SCK" (effects (font (size 1.016 1.016)))) (number "3" (effects (font (size 1.016 1.016)))))
			(pin bidirectional line (at -15.24 0 0) (length 5.08) (name "SDA" (effects (font (size 1.016 1.016)))) (number "4" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:PEC11R-4215F-S0024" (in_bom yes) (on_board yes)
		(property "Reference" "SW" (at -10.16 10.16 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "PEC11R-4215F-S0024" (at -10.16 -10.16 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "roadping-c3:XDCR_PEC11R-4215F-S0024" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Datasheet" "https://www.bourns.com/docs/Product-Datasheets/PEC11R.pdf" (at 0 0 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "PEC11R-4215F-S0024_0_0"
			(rectangle (start -7.62 -7.62) (end 7.62 7.62) (stroke (width 0.254) (type default)) (fill (type background)))
			(pin passive line (at -10.16 2.54 0) (length 2.54) (name "SW1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 10.16 2.54 180) (length 2.54) (name "SW2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 10.16 -2.54 180) (length 2.54) (name "A" (effects (font (size 1.016 1.016)))) (number "A" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 10.16 -5.08 180) (length 2.54) (name "B" (effects (font (size 1.016 1.016)))) (number "B" (effects (font (size 1.016 1.016)))))
			(pin passive line (at -10.16 -2.54 0) (length 2.54) (name "C" (effects (font (size 1.016 1.016)))) (number "C" (effects (font (size 1.016 1.016)))))
			(pin passive line (at -10.16 -5.08 0) (length 2.54) (name "SHIELD1" (effects (font (size 1.016 1.016)))) (number "S1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at -10.16 -7.62 0) (length 2.54) (name "SHIELD2" (effects (font (size 1.016 1.016)))) (number "S2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:R" (in_bom yes) (on_board yes)
		(property "Reference" "R" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "R" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Resistor_SMD:R_0603_1608Metric" (at 0 -5.08 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "R_0_1" (rectangle (start -1.905 0.762) (end 1.905 -0.762) (stroke (width 0.1524) (type default)) (fill (type none))))
		(symbol "R_1_1"
			(pin passive line (at -3.81 0 0) (length 1.905) (name "1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 3.81 0 180) (length 1.905) (name "2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:C" (in_bom yes) (on_board yes)
		(property "Reference" "C" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "C" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Capacitor_SMD:C_0603_1608Metric" (at 0 -5.08 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "C_0_1"
			(polyline (pts (xy -0.635 1.27) (xy -0.635 -1.27)) (stroke (width 0.2032) (type default)) (fill (type none)))
			(polyline (pts (xy 0.635 1.27) (xy 0.635 -1.27)) (stroke (width 0.2032) (type default)) (fill (type none))))
		(symbol "C_1_1"
			(pin passive line (at -3.81 0 0) (length 3.175) (name "1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 3.81 0 180) (length 3.175) (name "2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:Fuse" (in_bom yes) (on_board yes)
		(property "Reference" "F" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "Fuse/PTC" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Fuse:Fuse_1206_3216Metric" (at 0 -5.08 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "Fuse_0_1" (rectangle (start -1.905 0.762) (end 1.905 -0.762) (stroke (width 0.1524) (type default)) (fill (type none))))
		(symbol "Fuse_1_1"
			(pin passive line (at -3.81 0 0) (length 1.905) (name "1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 3.81 0 180) (length 1.905) (name "2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:D_TVS" (in_bom yes) (on_board yes)
		(property "Reference" "D" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "TVS" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Diode_SMD:D_SOD-323" (at 0 -5.08 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "D_TVS_0_1"
			(polyline (pts (xy -0.635 1.27) (xy -0.635 -1.27)) (stroke (width 0.2032) (type default)) (fill (type none)))
			(polyline (pts (xy -0.635 0) (xy 0.635 0.762) (xy 0.635 -0.762) (xy -0.635 0)) (stroke (width 0.2032) (type default)) (fill (type none))))
		(symbol "D_TVS_1_1"
			(pin passive line (at -3.81 0 0) (length 3.175) (name "A" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at 3.81 0 180) (length 3.175) (name "K" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:Conn_01x02" (in_bom yes) (on_board yes)
		(property "Reference" "J" (at 0 5.08 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "Conn_01x02" (at 0 -5.08 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" (at 0 -7.62 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "Conn_01x02_0_1" (rectangle (start -2.54 3.81) (end 2.54 -3.81) (stroke (width 0.254) (type default)) (fill (type background))))
		(symbol "Conn_01x02_1_1"
			(pin passive line (at -5.08 1.27 0) (length 2.54) (name "Pin_1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016)))))
			(pin passive line (at -5.08 -1.27 0) (length 2.54) (name "Pin_2" (effects (font (size 1.016 1.016)))) (number "2" (effects (font (size 1.016 1.016)))))
		)
	)
	(symbol "roadping-c3:TestPoint" (in_bom no) (on_board yes)
		(property "Reference" "TP" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "TestPoint" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Footprint" "roadping-c3:TP_Round_1.0mm" (at 0 -5.08 0) (hide yes) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "TestPoint_0_1" (circle (center 0 0) (radius 0.762) (stroke (width 0.1524) (type default)) (fill (type none))))
		(symbol "TestPoint_1_1" (pin passive line (at -3.81 0 0) (length 3.048) (name "1" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016))))))
	)
	(symbol "roadping-c3:PWR_FLAG" (in_bom no) (on_board no)
		(property "Reference" "#FLG" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "PWR_FLAG" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "PWR_FLAG_0_1" (polyline (pts (xy 0 0) (xy 0 2.54) (xy 1.27 1.905) (xy 0 1.27)) (stroke (width 0.2032) (type default)) (fill (type none))))
		(symbol "PWR_FLAG_1_1" (pin power_out line (at 0 0 270) (length 0) (name "pwr" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016))))))
	)
	(symbol "roadping-c3:GND" (in_bom no) (on_board no)
		(property "Reference" "#GND" (at 0 2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(property "Value" "GND" (at 0 -2.54 0) (show_name no) (do_not_autoplace no) (effects (font (size 1.27 1.27))))
		(symbol "GND_0_1"
			(polyline (pts (xy 0 0) (xy 2.54 0)) (stroke (width 0.2032) (type default)) (fill (type none)))
			(polyline (pts (xy 0.635 0) (xy 1.27 -0.889) (xy 1.905 0)) (stroke (width 0.2032) (type default)) (fill (type none))))
		(symbol "GND_1_1" (pin power_in line (at 0 0 270) (length 0) (name "GND" (effects (font (size 1.016 1.016)))) (number "1" (effects (font (size 1.016 1.016))))))
	)
'''

# ── Build schematic ──────────────────────────────────────────

def build():
    L = []
    L.append(f'(kicad_sch')
    L.append(f'\t(version 20260306)')
    L.append(f'\t(generator "eeschema")')
    L.append(f'\t(generator_version "10.0")')
    L.append(f'\t(uuid "{U()}")')
    L.append(f'\t(paper "A4")')
    L.append(f'\t(title_block')
    L.append(f'\t\t(title "RoadPing C3 Mini — USB-C + BQ24074")')
    L.append(f'\t\t(date "{date.today().isoformat()}")')
    L.append(f'\t\t(rev "schematic-draft")')
    L.append(f'\t\t(company "RoadPing")')
    L.append(f'\t)')
    L.append(f'\t(lib_symbols{EMBED}\t)')

    # ── SECTION 1: USB-C INPUT ────────────────────────────
    # J1 at (50,50), pins: VBUS, CC1, CC2, D+, D-, GND
    L.append(sym("Connector_USB:USB_C_Receptacle_USBIF", "J1", "TYPE-C-31-M-12",
        50, 50, fp="Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12",
        desc="USB-C receptacle 16-pin SMT"))
    # F1 PTC (1.1A) and D1 TVS
    L.append(78.74, 55.88, desc="1206L110SLWR"))
    L.append(76.2, 71.12, desc="TVS"))
    # CC pulldowns
    L.append(76.2, 78.74, desc="CC1"))
    L.append(76.2, 88.9, desc="CC2"))

    # Wire: J1 VBUS → F1 → VBUS label
    L.append(w(60.96, 50.8, 71.12, 50.8))    # J1 VBUS pin
    L.append(w(71.12, 50.8, 78.74, 55.88))    # to F1 pin1
    L.append(w(78.74, 55.88, 78.74, 66.04))    # F1 pin2 down
    L.append(gl("80", 78.74, 66.04))   # label ON wire end

    # Wire: D1 TVS
    L.append(w(76.2, 66.04, 76.2, 71.12))    # VBUS to D1
    L.append(pwr_sym("75", 76.2, 73.66))

    # CC1/CC2 pulldown wires
    L.append(ll("65", 66.04, 78.74))
    L.append(w(66.04, 78.74, 76.2, 78.74))    # label on wire
    L.append(pwr_sym("75", 76.2, 83.82000000000001))
    L.append(ll("65", 66.04, 88.9))
    L.append(w(66.04, 88.9, 76.2, 88.9))
    L.append(pwr_sym("75", 76.2, 91.44))

    # USBLC6-2SC6
    L.append(sym("Interface_Protection:USBLC6-2SC6", "D2", "USBLC6-2SC6",
        105, 55, desc="USB ESD"))
    # J1 D+/D- → D2
    L.append(ll("65", 66.04, 43.18))
    L.append(w(66.04, 43.18, 93.98, 43.18))    # to D2 pin1
    L.append(w(66.04, 38.1, 93.98, 38.1))    # to D2 pin3
    L.append(ll("65", 66.04, 38.1))
    # D2 VBUS bias
    L.append(gl("95", 93.98, 55.88))

    # ESP labels for USB coming from D2
    L.append(gl("115", 114.3, 43.18))
    L.append(gl("115", 114.3, 38.1))

    # PWR_FLAG on VBUS
    L.append(pwr_sym("80", 78.74, 68.58))

    # ── SECTION 2: BQ24074 CHARGER ───────────────────────
    L.append(sym("Battery_Management:BQ24074_RGT", "U1", "BQ24074RGTR",
        50, 120, fp="Package_DFN_QFN:WQFN-20-1EP_3.5x3.5mm_P0.5mm_EP2.7x2.7mm",
        ds="https://www.ti.com/lit/ds/symlink/bq24074.pdf",
        desc="LiPo charger 1A power-path"))

    # VBUS from section 1
    L.append(gl("40", 40.64, 119.38))
    L.append(w(40.64, 119.38, 50.8, 119.38))

    # Passives
    L.append(35.56, 137.16, desc="ISET"))
    L.append(35.56, 142.24, desc="ILIM"))
    L.append(71.12, 127.0, desc="TS PU"))
    L.append(71.12, 132.08, desc="TS PD"))
    L.append(pwr_sym("35", 35.56, 134.62))
    L.append(pwr_sym("35", 35.56, 139.7))
    L.append(pwr_sym("72", 71.12, 129.54))

    # Charger status pull-ups
    L.append(76.2, 111.76, desc="CE PU"))
    L.append(76.2, 106.68, desc="CHG PU"))
    L.append(76.2, 101.6, desc="FAULT PU"))
    L.append(gl("82", 81.28, 114.3))
    L.append(gl("82", 81.28, 111.76))
    L.append(gl("82", 81.28, 106.68))
    L.append(gl("82", 81.28, 101.6))

    # SYS_RAW and VBAT
    L.append(gl("75", 76.2, 119.38))
    L.append(gl("75", 76.2, 127.0))
    L.append(pwr_sym("80", 78.74, 121.92))
    L.append(pwr_sym("80", 78.74, 129.54))

    # ── SECTION 3: AP2112K + LiPo ─────────────────────────
    L.append(139.7, 119.38, desc="LDO"))
    L.append(129.54, 129.54, desc="VIN cap"))
    L.append(129.54, 134.62, desc="VOUT cap"))
    L.append(154.94, 129.54, desc="3V3 bulk"))
    L.append(160.02, 119.38, desc="VBAT PTC"))
    L.append(170.18, 124.46000000000001,
        fp="Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical", desc="LiPo 2000mAh"))

    L.append(gl("130", 129.54, 119.38))
    L.append(gl("155", 154.94, 116.84))
    L.append(gl("155", 154.94, 124.46000000000001))
    L.append(pwr_sym("130", 129.54, 127.0))
    L.append(pwr_sym("130", 129.54, 132.08))
    L.append(pwr_sym("155", 154.94, 127.0))
    L.append(pwr_sym("170", 170.18, 121.92))
    L.append(pwr_sym("158", 157.48, 121.92))

    # ── SECTION 4: ESP32-C3-MINI-1 ────────────────────────
    MX, MY = 250, 50
    L.append(sym("roadping-c3:ESP32-C3-MINI-1-N4", "U3", "ESP32-C3-MINI-1-N4",
        MX, MY, desc="RISC-V BLE 5 module 4MB"))

    # GPIO connections via wire stubs to global labels
    # Each GPIO pin tip is at: MX-20.32 for left side, MY+pin_y
    # Wire from pin tip outward by 2.54, place label on wire end
    left_pins = [
        (0, 25.4, "GPIO0"),
        (1, 22.86, "GPIO1"),
        (2, 20.32, "GPIO2"),
        (3, 17.78, "GPIO3"),
        (4, 15.24, "GPIO4"),
        (5, 12.7, "GPIO5"),
        (6, 10.16, "GPIO6"),
        (7, 7.62, "GPIO7"),
        (8, 5.08, "I2C_SDA"),
        (9, 2.54, "GPIO9"),
        (10, 0, "I2C_SCL"),
        (18, -15.24, "GPIO18"),
        (19, -17.78, "GPIO19"),
        (20, -20.32, "GPIO20"),
        (21, -22.86, "GPIO21"),
    ]
    for pin_num, pin_y, label_name in left_pins:
        tip_x = MX - 20.32  # pin tip x
        tip_y = MY + pin_y
        wire_x = tip_x - G   # 2.54mm wire stub left
        L.append(w(wire_x, tip_y, tip_x, tip_y))
        L.append(gl(label_name, wire_x, tip_y))

    # Right side: power pins → 3V3, USB
    right_pins = [
        (25.4, "VDD"),
        (22.86, "VDD3P3"),
        (20.32, "VDD3P3_RTC"),
        (17.78, "VDD3P3_CPU"),
        (15.24, "VDD3P3_USB"),
        (12.7, "VDD3P3_RTC2"),
    ]
    for pin_y, lbl_name in right_pins:
        tip_x = MX + 20.32
        tip_y = MY + pin_y
        wire_x = tip_x + G
        L.append(w(tip_x, tip_y, wire_x, tip_y))
        L.append(gl("3V3", wire_x, tip_y))

    # GND pins (right side)
    for pin_y in [-5.08, -7.62, -10.16]:
        tip_x = MX + 20.32
        tip_y = MY + pin_y
        wire_x = tip_x + G
        L.append(w(tip_x, tip_y, wire_x, tip_y))
        L.append(pwr_sym("GND", wire_x+2, tip_y))

    # USB D+/D- on right side
    for pin_y, label_name in [(-22.86, "USB_DP"), (-25.4, "USB_DN")]:
        tip_x = MX + 20.32
        tip_y = MY + pin_y
        wire_x = tip_x + G
        L.append(w(tip_x, tip_y, wire_x, tip_y))
        L.append(gl(label_name, wire_x, tip_y))

    # CHIP_EN on left bottom
    tip_x = MX - 20.32
    tip_y = MY - 27.94
    wire_x = tip_x - G
    L.append(w(wire_x, tip_y, tip_x, tip_y))

    # Labels that connect to charger/peripherals (match names)
    L.append(gl("CE_CHG", MX-35, MY+10))
    L.append(gl("CHG_STAT", MX-35, MY+7))
    L.append(gl("FLT_STAT", MX-35, MY+4))
    L.append(gl("I2C_SDA", MX-30, MY+2))
    L.append(gl("I2C_SCL", MX-30, MY-1))
    # Encoder from GPIO2/3/4
    L.append(gl("GPIO2", MX-30, MY+15))
    L.append(gl("GPIO3", MX-30, MY+12))
    L.append(gl("GPIO4", MX-30, MY+9))

    # ── SECTION 5: PERIPHERALS ────────────────────────────
    # OLED
    L.append(88.9, 243.84, desc="I2C OLED"))
    L.append(gl("105", 104.14, 243.84))
    L.append(pwr_sym("105", 104.14, 238.76))
    # I2C from ESP labels
    L.append(gl("80", 78.74, 243.84))
    L.append(gl("80", 78.74, 241.3))

    # Encoder PEC11R
    L.append(154.94, 243.84, desc="Encoder"))
    L.append(144.78, 241.3, desc="EncA"))
    L.append(144.78, 238.76, desc="EncB"))
    L.append(144.78, 233.68, desc="EncSW"))
    L.append(gl("138", 137.16, 243.84))
    L.append(gl("170", 170.18, 243.84))
    L.append(gl("170", 170.18, 238.76))
    L.append(gl("170", 170.18, 236.22))

    # Gated ADC
    L.append(88.9, 269.24,
        fp="Package_TO_SOT_SMD:SOT-23", desc="ADC gate FET"))
    L.append(78.74, 269.24, desc="ADC top"))
    L.append(78.74, 271.78000000000003, desc="ADC bottom"))
    L.append(78.74, 276.86, desc="Gate PD"))
    L.append(104.14, 269.24, desc="ADC filter"))
    L.append(gl("75", 76.2, 266.7))
    L.append(gl("110", 109.22, 269.24))
    L.append(gl("110", 109.22, 271.78000000000003))
    L.append(pwr_sym("105", 104.14, 266.7))
    L.append(pwr_sym("80", 78.74, 271.78000000000003))
    L.append(pwr_sym("80", 78.74, 279.4))

    # BOOT button
    L.append(210.82, 269.24, desc="Momentary"))
    L.append(gl("205", 205.74, 269.24))
    L.append(pwr_sym("210", 210.82, 266.7))

    # ── SECTION 6: TEST POINTS ───────────────────────────
    L.append(279.4, 248.92000000000002))
    L.append(279.4, 243.84))
    L.append(gl("275", 274.32, 248.92000000000002))
    L.append(gl("275", 274.32, 241.3))

    # PWR_FLAG on buses
    L.append(pwr_sym("158", 157.48, 114.3))

    L.append(f'\t(sheet_instances')
    L.append(f'\t\t(path "/" (page "1"))')
    L.append(f'\t)')
    L.append(f'\t(symbol_instances)')
    L.append(f')')
    return '\n'.join(L) + '\n'

if __name__ == "__main__":
    s = build()
    with open(SCH, "w") as f:
        f.write(s)
    print(f"✅ {SCH} — {len(s)} bytes, {len(s.splitlines())} lines")
