#!/usr/bin/env python3
"""Generate RoadPing C3 Mini 40x40mm PCB skeleton (KiCad 10)"""
import os, uuid as _u

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")
U = lambda: str(_u.uuid4())

L = []
L.append('(kicad_pcb')
L.append('\t(version 20260206)')
L.append('\t(generator "pcbnew")')
L.append('\t(generator_version "10.0")')
L.append('\t(general')
L.append('\t\t(thickness 1.6)')
L.append('\t\t(legacy_teardrops no)')
L.append('\t)')

# ── Layers ──
L.append('\t(layers')
L.append('\t\t(0 "F.Cu" signal)')
L.append('\t\t(4 "In1.Cu" signal "In1.Cu(GND)")')
L.append('\t\t(6 "In2.Cu" signal "In2.Cu(PWR)")')
L.append('\t\t(2 "B.Cu" signal)')
L.append('\t\t(9 "F.Adhes" user "F.Adhesive")')
L.append('\t\t(11 "B.Adhes" user "B.Adhesive")')
L.append('\t\t(13 "F.Paste" user)')
L.append('\t\t(15 "B.Paste" user)')
L.append('\t\t(5 "F.SilkS" user "F.Silkscreen")')
L.append('\t\t(7 "B.SilkS" user "B.Silkscreen")')
L.append('\t\t(1 "F.Mask" user)')
L.append('\t\t(3 "B.Mask" user)')
L.append('\t\t(17 "Dwgs.User" user "User.Drawings")')
L.append('\t\t(19 "Cmts.User" user "User.Comments")')
L.append('\t\t(21 "Eco1.User" user "User.Eco1")')
L.append('\t\t(23 "Eco2.User" user "User.Eco2")')
L.append('\t\t(25 "Edge.Cuts" user)')
L.append('\t\t(27 "Margin" user)')
L.append('\t\t(31 "F.CrtYd" user "F.Courtyard")')
L.append('\t\t(29 "B.CrtYd" user "B.Courtyard")')
L.append('\t\t(35 "F.Fab" user)')
L.append('\t\t(33 "B.Fab" user)')
L.append('\t)')

# ── Stackup ──
L.append('\t(setup')
L.append('\t\t(stackup')
L.append('\t\t\t(layer "F.SilkS" (type "Top Silk Screen"))')
L.append('\t\t\t(layer "F.Paste" (type "Top Solder Paste"))')
L.append('\t\t\t(layer "F.Mask" (type "Top Solder Mask") (color "Green") (thickness 0.01))')
L.append('\t\t\t(layer "F.Cu" (type "copper") (thickness 0.035))')
L.append('\t\t\t(layer "dielectric 1" (type "prepreg") (thickness 0.18) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))')
L.append('\t\t\t(layer "In1.Cu" (type "copper") (thickness 0.035))')
L.append('\t\t\t(layer "dielectric 2" (type "core") (thickness 1.08) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))')
L.append('\t\t\t(layer "In2.Cu" (type "copper") (thickness 0.035))')
L.append('\t\t\t(layer "dielectric 3" (type "prepreg") (thickness 0.18) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))')
L.append('\t\t\t(layer "B.Cu" (type "copper") (thickness 0.035))')
L.append('\t\t\t(layer "B.Mask" (type "Bottom Solder Mask") (color "Green") (thickness 0.01))')
L.append('\t\t\t(layer "B.Paste" (type "Bottom Solder Paste"))')
L.append('\t\t\t(layer "B.SilkS" (type "Bottom Silk Screen"))')
L.append('\t\t\t(copper_finish "None")')
L.append('\t\t\t(dielectric_constraints no)')
L.append('\t\t)')

# ── Design rules ──
L.append('\t\t(pad_to_mask_clearance 0)')
L.append('\t\t(allow_soldermask_bridges_in_footprints no)')
L.append('\t\t(tenting (front yes) (back yes))')
L.append('\t\t(covering (front no) (back no))')
L.append('\t\t(plugging (front no) (back no))')
L.append('\t\t(capping no)')
L.append('\t\t(filling no)')
L.append('\t\t(clearance_default 0.2)')
L.append('\t\t(track_width_default 0.25)')
L.append('\t\t(via_rule')
L.append('\t\t\t(via_diameter_default 0.6)')
L.append('\t\t\t(via_drill_default 0.3)')
L.append('\t\t\t(hole_to_hole_min 0.5)')
L.append('\t\t)')

# ── Net classes ──
L.append('\t\t(net_classes')
L.append('\t\t\t(net_class "Default" (clearance 0.2) (trace_width 0.25) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "POWER_HIGH" (clearance 0.2) (trace_width 0.5) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "VBUS") (add_net "VBUS_FILT") (add_net "SYS_RAW")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "POWER_BAT" (clearance 0.2) (trace_width 0.5) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "VBAT") (add_net "VBAT_FILT")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "POWER_3V3" (clearance 0.2) (trace_width 0.4) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "3V3")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "GND" (clearance 0.2) (trace_width 0.4) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "GND")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "USB_DIFF" (clearance 0.15) (trace_width 0.3) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "USB_DP") (add_net "USB_DN")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t\t(net_class "ANALOG" (clearance 0.25) (trace_width 0.25) (via_dia 0.6) (via_drill 0.3)')
L.append('\t\t\t\t(add_net "BAT_ADC")')
L.append('\t\t\t\t(uuid "' + U() + '")')
L.append('\t\t\t)')
L.append('\t\t)')

# ── PCB plot settings ──
L.append('\t\t(pcbplotparams')
L.append('\t\t\t(layerselection 0x00000000_00000000_55555555_5755f5ff)')
L.append('\t\t\t(plot_on_all_layers_selection 0x00000000_00000000_00000000_00000000)')
L.append('\t\t\t(disableapertmacros no)')
L.append('\t\t\t(usegerberextensions no)')
L.append('\t\t\t(usegerberattributes yes)')
L.append('\t\t\t(usegerberadvancedattributes yes)')
L.append('\t\t\t(creategerberjobfile yes)')
L.append('\t\t\t(dashed_line_dash_ratio 12)')
L.append('\t\t\t(dashed_line_gap_ratio 3)')
L.append('\t\t\t(svgprecision 4)')
L.append('\t\t\t(plotframeref no)')
L.append('\t\t\t(mode 1)')
L.append('\t\t\t(useauxorigin no)')
L.append('\t\t\t(pdf_front_fp_property_popups yes)')
L.append('\t\t\t(pdf_back_fp_property_popups yes)')
L.append('\t\t\t(pdf_metadata yes)')
L.append('\t\t\t(pdf_single_document no)')
L.append('\t\t\t(dxfpolygonmode yes)')
L.append('\t\t\t(dxfimperialunits yes)')
L.append('\t\t\t(dxfusepcbnewfont yes)')
L.append('\t\t\t(psnegative no)')
L.append('\t\t\t(psa4output no)')
L.append('\t\t\t(plot_black_and_white yes)')
L.append('\t\t\t(sketchpadsonfab no)')
L.append('\t\t\t(plotpadnumbers no)')
L.append('\t\t\t(hidednponfab no)')
L.append('\t\t\t(sketchdnponfab yes)')
L.append('\t\t\t(crossoutdnponfab yes)')
L.append('\t\t\t(subtractmaskfromsilk no)')
L.append('\t\t\t(outputformat 1)')
L.append('\t\t\t(mirror no)')
L.append('\t\t\t(drillshape 1)')
L.append('\t\t\t(scaleselection 1)')
L.append('\t\t\t(outputdirectory "")')
L.append('\t\t)')
L.append('\t)') # end setup

# ── Board outline: 40x40mm on Edge.Cuts ──
BX, BY = 0, 0
BW, BH = 40, 40

def edge_line(x1, y1, x2, y2):
    return '\t(gr_line (start {} {}) (end {} {}) (layer "Edge.Cuts") (stroke (width 0.05) (type default)) (uuid "{}"))'.format(x1, y1, x2, y2, U())

# Simple rectangle for now (rounded corners can be added in KiCad GUI)
L.append(edge_line(BX, BY, BW, BY))          # bottom edge
L.append(edge_line(BW, BY, BW, BH))          # right edge
L.append(edge_line(BW, BH, BX, BH))          # top edge
L.append(edge_line(BX, BH, BX, BY))          # left edge

# ── Paper size (for printing / plotting) ──
L.append('\t(paper "A4")')
L.append('\t(title_block')
L.append('\t\t(title "RoadPing C3 Mini - USB-C + BQ24074")')
L.append('\t\t(date "2026-06-17")')
L.append('\t\t(rev "v1")')
L.append('\t)')

# ── Page number ──
L.append('\t(page_info')
L.append('\t\t(number "1")')
L.append('\t)')

# ── Close root element ──
L.append(')')

# Write file
with open(PCB, 'w') as f:
    f.write('\n'.join(L) + '\n')

print(f"✅ Written {PCB} — {len(L)} lines, {BW}x{BH}mm, {4} layers")
print(f"   Layer stack: F.Cu / In1.Cu(GND) / In2.Cu(PWR) / B.Cu")
print(f"   Net classes: Default, POWER_HIGH, POWER_BAT, POWER_3V3, GND, USB_DIFF, ANALOG")
print(f"   Board outline: {BW}x{BH}mm rectangle on Edge.Cuts")
