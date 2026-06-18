#!/usr/bin/env python3
import argparse
import json
import uuid
from pathlib import Path

try:
    import pcbnew
except ImportError:  # pragma: no cover - text-only cleanup runs on system Python
    pcbnew = None


POWER_NETS = {"/USB_5V_IN", "/USB_5V", "/BAT_CONN+", "/VBAT", "/SYS_RAW"}
THREEV3_NETS = {"/3V3"}
GND_NETS = {"/GND"}


def mm(value):
    return pcbnew.FromMM(value)


def point(x_mm, y_mm):
    return pcbnew.VECTOR2I(mm(x_mm), mm(y_mm))


def layer_name(board, layer):
    return board.GetLayerName(layer)


def fp_map(board):
    return {fp.GetReference(): fp for fp in board.GetFootprints()}


def net_name(item):
    net = item.GetNet()
    return net.GetNetname() if net else ""


ITERATIONS = {
    "iter1": {
        "bounds": (12.6, 12.8, 65.1, 54.8),
        "keepout": (13.1, 31.9, 15.7, 52.6),
        "moves": {
            "U1": (19.7, 31.4, 0.0),
            "U3": (24.5, 42.6, -90.0),
            "DS1": (40.0, 37.81, -90.0),
            "SW1": (57.2, 31.0, 0.0),
            "J1": (24.0, 15.5, 0.0),
            "J2": (52.5, 16.3, -90.0),
            "J3": (59.5, 52.8, 180.0),
            "D1": (29.5, 21.7, 90.0),
            "F1": (27.5, 18.0, 90.0),
            "F2": (49.0, 18.0, 180.0),
            "MH2": (16.4, 16.3, 0.0),
            "MH3": (61.8, 16.3, 0.0),
            "MH4": (61.8, 44.4, 0.0),
            "MH1": (38.8, 51.4, 0.0),
            "U2": (31.2, 43.3, -90.0),
            "C1": (34.0, 43.3, 90.0),
            "C2": (27.5, 45.6, 0.0),
            "C3": (33.8, 46.3, 90.0),
            "C4": (30.6, 47.5, 0.0),
            "C5": (42.4, 35.3, 90.0),
            "R6": (17.7, 46.5, 90.0),
            "R7": (20.1, 46.5, 90.0),
            "C9": (22.5, 46.5, 90.0),
            "R3": (54.7, 35.7, -90.0),
            "R4": (59.7, 35.7, -90.0),
            "R5": (59.7, 27.3, 90.0),
        },
        "testpads": {
            "TP8": (43.0, 47.1, 180.0, "SDA"),
            "TP13": (45.4, 47.1, 180.0, "ADC"),
            "TP3": (47.8, 47.1, 180.0, "RAW"),
            "TP2": (50.2, 47.1, 180.0, "BAT"),
            "TP5": (52.6, 47.1, 180.0, "GND"),
            "TP1": (55.0, 47.1, 180.0, "5V"),
            "TP10": (57.4, 47.1, 180.0, "A"),
            "TP11": (59.8, 47.1, 180.0, "B"),
            "TP12": (62.2, 47.1, 180.0, "SW"),
        },
        "boot": (24.24, 32.4),
        "gnd_stitches": [
            (16.8, 31.0),
            (37.0, 32.4),
            (47.0, 21.0),
            (46.5, 41.8),
            (52.0, 41.8),
            (61.5, 48.0),
        ],
    },
    "iter2": {
        "bounds": (12.6, 12.8, 65.1, 53.7),
        "keepout": (13.1, 33.2, 15.7, 53.5),
        "moves": {
            "U1": (19.7, 31.4, 0.0),
            "U3": (24.5, 43.5, -90.0),
            "DS1": (40.0, 37.81, -90.0),
            "SW1": (55.85, 30.2, 0.0),
            "J1": (16.0, 15.5, 0.0),
            "J2": (54.0, 16.3, -90.0),
            "J3": (60.0, 51.7, 180.0),
            "D1": (18.0, 21.5, 90.0),
            "F1": (18.0, 25.0, 90.0),
            "F2": (46.0, 22.2, 180.0),
            "MH2": (16.4, 16.3, 0.0),
            "MH3": (60.4, 16.3, 0.0),
            "MH4": (60.4, 43.1, 0.0),
            "MH1": (40.8, 50.3, 0.0),
            "U2": (31.2, 44.2, -90.0),
            "C1": (34.0, 44.2, 90.0),
            "C2": (27.5, 46.5, 0.0),
            "C3": (33.8, 47.2, 90.0),
            "C4": (30.6, 48.4, 0.0),
            "C5": (42.4, 35.3, 90.0),
            "R6": (17.7, 47.4, 90.0),
            "R7": (20.1, 47.4, 90.0),
            "C9": (22.5, 47.4, 90.0),
            "R3": (53.35, 34.9, -90.0),
            "R4": (58.35, 34.9, -90.0),
            "R5": (58.35, 26.5, 90.0),
        },
        "testpads": {
            "TP8": (43.0, 46.3, 180.0, "SDA"),
            "TP13": (45.4, 46.3, 180.0, "ADC"),
            "TP3": (47.8, 46.3, 180.0, "RAW"),
            "TP2": (50.2, 46.3, 180.0, "BAT"),
            "TP5": (52.6, 46.3, 180.0, "GND"),
            "TP1": (55.0, 46.3, 180.0, "5V"),
            "TP10": (57.4, 46.3, 180.0, "A"),
            "TP11": (59.8, 46.3, 180.0, "B"),
            "TP12": (62.2, 46.3, 180.0, "SW"),
        },
        "boot": (24.24, 32.7),
        "gnd_stitches": [
            (16.8, 31.0),
            (37.0, 32.4),
            (46.5, 41.8),
            (52.0, 41.8),
            (61.5, 46.8),
        ],
    },
}


def summary(board):
    fps = list(board.GetFootprints())
    tracks = list(board.GetTracks())
    x0, y0, x1, y1 = board_outline_bbox(board)
    return {
        "footprints": len(fps),
        "top_footprints": sum(1 for fp in fps if fp.GetLayer() == pcbnew.F_Cu),
        "bottom_footprints": sum(1 for fp in fps if fp.GetLayer() == pcbnew.B_Cu),
        "track_segments": sum(1 for item in tracks if not isinstance(item, pcbnew.PCB_VIA)),
        "vias": sum(1 for item in tracks if isinstance(item, pcbnew.PCB_VIA)),
        "zones": board.GetAreaCount(),
        "width_mm": round(x1 - x0, 3),
        "height_mm": round(y1 - y0, 3),
        "area_mm2": round((x1 - x0) * (y1 - y0), 1),
    }


def board_outline_bbox(board):
    xs = []
    ys = []
    for drawing in board.GetDrawings():
        if hasattr(drawing, "GetLayer") and drawing.GetLayer() == pcbnew.Edge_Cuts:
            if hasattr(drawing, "GetStart") and hasattr(drawing, "GetEnd"):
                for p in (drawing.GetStart(), drawing.GetEnd()):
                    xs.append(pcbnew.ToMM(p.x))
                    ys.append(pcbnew.ToMM(p.y))
    if not xs:
        return (0.0, 0.0, 0.0, 0.0)
    return (min(xs), min(ys), max(xs), max(ys))


def set_footprint(fp, x_mm, y_mm, angle_deg):
    fp.SetPosition(point(x_mm, y_mm))
    fp.SetOrientationDegrees(angle_deg)


def remove_routes(board):
    removed = {"tracks": 0, "vias": 0}
    for item in list(board.GetTracks()):
        if isinstance(item, pcbnew.PCB_VIA):
            removed["vias"] += 1
        else:
            removed["tracks"] += 1
        board.Remove(item)
    return removed


def remove_board_text(board):
    for drawing in list(board.GetDrawings()):
        if isinstance(drawing, pcbnew.PCB_TEXT):
            board.Remove(drawing)


def remove_edge_cuts(board):
    for drawing in list(board.GetDrawings()):
        if hasattr(drawing, "GetLayer") and drawing.GetLayer() == pcbnew.Edge_Cuts:
            board.Remove(drawing)


def add_edge_segment(board, start, end):
    shape = pcbnew.PCB_SHAPE(board)
    shape.SetShape(pcbnew.SHAPE_T_SEGMENT)
    shape.SetLayer(pcbnew.Edge_Cuts)
    shape.SetStart(point(*start))
    shape.SetEnd(point(*end))
    shape.SetWidth(mm(0.1))
    board.Add(shape)


def rebuild_outline(board, bounds):
    x0, y0, x1, y1 = bounds
    remove_edge_cuts(board)
    add_edge_segment(board, (x0, y0), (x1, y0))
    add_edge_segment(board, (x1, y0), (x1, y1))
    add_edge_segment(board, (x1, y1), (x0, y1))
    add_edge_segment(board, (x0, y1), (x0, y0))


def remove_zones(board):
    for zone in list(board.Zones()):
        board.Remove(zone)


def add_rect_outline(zone, bounds):
    x0, y0, x1, y1 = bounds
    outline = zone.Outline()
    outline.NewOutline()
    for x_mm, y_mm in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        outline.Append(point(x_mm, y_mm))


def add_gnd_zone(board, layer, bounds, name):
    zone = pcbnew.ZONE(board)
    zone.SetZoneName(name)
    zone.SetLayer(layer)
    zone.SetNet(board.FindNet("/GND"))
    zone.SetLocalClearance(mm(0.254))
    zone.SetMinThickness(mm(0.254))
    add_rect_outline(zone, bounds)
    board.Add(zone)


def add_antenna_keepout(board, layer, keepout, name):
    zone = pcbnew.ZONE(board)
    zone.SetZoneName(name)
    zone.SetLayer(layer)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowPads(True)
    zone.SetDoNotAllowZoneFills(True)
    zone.SetDoNotAllowFootprints(False)
    add_rect_outline(zone, keepout)
    board.Add(zone)


def rebuild_zones(board, bounds, keepout):
    remove_zones(board)
    add_gnd_zone(board, pcbnew.F_Cu, bounds, "EVT_V2_GND_ZONE_FRONT")
    add_antenna_keepout(board, pcbnew.F_Cu, keepout, "U3_ANTENNA_KEEPOUT_FRONT")
    add_antenna_keepout(board, pcbnew.B_Cu, keepout, "U3_ANTENNA_KEEPOUT_BACK")
    add_gnd_zone(board, pcbnew.B_Cu, bounds, "EVT_V2_GND_ZONE_BACK")


def add_text(board, text_value, x_mm, y_mm, layer, angle=0.0, size=0.75, mirrored=False):
    text = pcbnew.PCB_TEXT(board)
    text.SetText(text_value)
    text.SetPosition(point(x_mm, y_mm))
    text.SetLayer(layer)
    text.SetTextSize(point(size, size))
    text.SetTextThickness(mm(0.12))
    text.SetTextAngleDegrees(angle)
    text.SetMirrored(mirrored)
    board.Add(text)


def add_labels(board, config):
    remove_board_text(board)

    add_text(board, "ROADPING C3 EVT2", 42.0, 52.45, pcbnew.F_SilkS, 0.0, 0.7)
    add_text(board, "OLED", 42.7, 32.55, pcbnew.F_SilkS, 0.0, 0.7)
    for label, y_mm in (("3V3", 34.0), ("GND", 36.54), ("SCL", 39.08), ("SDA", 41.62)):
        add_text(board, label, 42.7, y_mm, pcbnew.F_SilkS, 0.0, 0.7)

    add_text(board, "USB", 26.0, 13.85, pcbnew.F_SilkS, 0.0, 0.65)
    add_text(board, "GND", 26.0, 18.25, pcbnew.F_SilkS, 0.0, 0.65)
    add_text(board, "BAT+", 56.7, 15.25, pcbnew.F_SilkS, 0.0, 0.65)
    add_text(board, "BAT-", 56.7, 18.25, pcbnew.F_SilkS, 0.0, 0.65)
    add_text(board, "D1K", 31.2, 21.7, pcbnew.F_SilkS, 0.0, 0.6)

    for ref, (x_mm, y_mm, _angle, label) in config["testpads"].items():
        add_text(board, label, x_mm, y_mm + 1.45, pcbnew.F_SilkS, 0.0, 0.62)

    boot_x, boot_y = config["boot"]
    add_text(board, "BOOT", boot_x + 1.7, boot_y, pcbnew.F_SilkS, 0.0, 0.65)

    f1 = config["moves"]["F1"]
    f2 = config["moves"]["F2"]
    add_text(board, "F1 USB", f1[0] - 2.6, f1[1], pcbnew.B_SilkS, 90.0, 0.65, True)
    add_text(board, "F2 BAT", f2[0], f2[1] + 2.8, pcbnew.B_SilkS, 0.0, 0.65, True)


def boot_source(u3):
    gpio9 = next(pad for pad in u3.Pads() if pad.GetNumber() == "9")
    return gpio9.GetPosition(), gpio9.GetNetname()


def add_boot_probe(board, source_position, net_name_value, x_mm, y_mm):
    net = board.FindNet(net_name_value)

    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(x_mm, y_mm))
    via.SetWidth(mm(1.2))
    via.SetDrill(mm(0.5))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    if hasattr(via, "SetFrontTenting"):
        via.SetFrontTenting(False)
        via.SetBackTenting(False)
    board.Add(via)

    track = pcbnew.PCB_TRACK(board)
    track.SetStart(source_position)
    track.SetEnd(point(x_mm, y_mm))
    track.SetWidth(mm(0.25))
    track.SetLayer(pcbnew.B_Cu)
    track.SetNet(net)
    board.Add(track)


def add_gnd_stitches(board, stitches):
    net = board.FindNet("/GND")
    for x_mm, y_mm in stitches:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(point(x_mm, y_mm))
        via.SetWidth(mm(0.6))
        via.SetDrill(mm(0.3))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(net)
        board.Add(via)


def apply_config(source, destination, config_name, clear_routes=True):
    config = ITERATIONS[config_name]
    board = pcbnew.LoadBoard(str(source))
    before = summary(board)
    footprints = fp_map(board)

    for ref, placement in config["moves"].items():
        set_footprint(footprints[ref], *placement)

    for ref, placement in config["testpads"].items():
        x_mm, y_mm, angle_deg, _label = placement
        set_footprint(footprints[ref], x_mm, y_mm, angle_deg)

    pcbnew.SaveBoard(str(destination), board)
    removed = {"tracks": 0, "vias": 0}
    if clear_routes:
        routed = pcbnew.LoadBoard(str(destination))
        boot_position, boot_net = boot_source(fp_map(routed)["U3"])
        removed = remove_routes(routed)
        add_boot_probe(routed, boot_position, boot_net, *config["boot"])
        add_gnd_stitches(routed, config["gnd_stitches"])
        pcbnew.SaveBoard(str(destination), routed)

    try:
        after = summary(routed if clear_routes else board)
    except Exception as exc:  # KiCad's SWIG bindings can fail during cleanup/reload.
        after = {"summary_unavailable": str(exc)}
    print(
        json.dumps(
            {
                "source": str(source),
                "destination": str(destination),
                "iteration": config_name,
                "before": before,
                "after": after,
                "removed_routes": removed,
                "bounds": config["bounds"],
                "keepout": config["keepout"],
                "moved_refs": sorted(list(config["moves"].keys()) + list(config["testpads"].keys())),
            },
            indent=2,
        )
    )


def export_dsn(board_path, dsn_path):
    board = pcbnew.LoadBoard(str(board_path))
    if not pcbnew.ExportSpecctraDSN(board, str(dsn_path)):
        raise RuntimeError(f"Failed to export DSN: {dsn_path}")
    print(json.dumps({"board": str(board_path), "dsn": str(dsn_path)}, indent=2))


def import_ses(board_path, ses_path, output_path):
    board = pcbnew.LoadBoard(str(board_path))
    if not pcbnew.ImportSpecctraSES(board, str(ses_path)):
        raise RuntimeError(f"Failed to import SES: {ses_path}")
    pcbnew.SaveBoard(str(output_path), board)
    print(json.dumps({"board": str(board_path), "ses": str(ses_path), "output": str(output_path), "summary": summary(board)}, indent=2))


def set_widths(board_path, output_path):
    board = pcbnew.LoadBoard(str(board_path))
    changed = {"power": 0, "threev3": 0, "gnd": 0}
    for item in board.GetTracks():
        if isinstance(item, pcbnew.PCB_VIA):
            continue
        name = net_name(item)
        if name in POWER_NETS:
            item.SetWidth(mm(0.5))
            changed["power"] += 1
        elif name in THREEV3_NETS:
            item.SetWidth(mm(0.4))
            changed["threev3"] += 1
        elif name in GND_NETS:
            item.SetWidth(mm(0.4))
            changed["gnd"] += 1
    pcbnew.SaveBoard(str(output_path), board)
    print(json.dumps({"board": str(board_path), "output": str(output_path), "changed": changed, "summary": summary(board)}, indent=2))


def inspect(board_path):
    board = pcbnew.LoadBoard(str(board_path))
    data = {
        "summary": summary(board),
        "outline_bbox": board_outline_bbox(board),
        "footprints": {},
        "zones": [],
    }
    for ref, fp in sorted(fp_map(board).items()):
        data["footprints"][ref] = {
            "layer": layer_name(board, fp.GetLayer()),
            "x": round(pcbnew.ToMM(fp.GetPosition().x), 3),
            "y": round(pcbnew.ToMM(fp.GetPosition().y), 3),
            "angle": round(fp.GetOrientationDegrees(), 3),
        }
    for zone in board.Zones():
        data["zones"].append(
            {
                "name": zone.GetZoneName(),
                "layer": layer_name(board, zone.GetLayer()),
                "rule_area": zone.GetIsRuleArea(),
                "net": zone.GetNetname(),
            }
        )
    print(json.dumps(data, indent=2))


def paren_delta(line):
    delta = 0
    in_string = False
    escaped = False
    for ch in line:
        if escaped:
            escaped = False
            continue
        if ch == "\\" and in_string:
            escaped = True
            continue
        if ch == '"':
            in_string = not in_string
            continue
        if in_string:
            continue
        if ch == "(":
            delta += 1
        elif ch == ")":
            delta -= 1
    return delta


def top_level_head(line):
    stripped = line.lstrip()
    if not stripped.startswith("("):
        return None
    if not (line.startswith("\t(") or line.startswith("  (")):
        return None
    return stripped[1:].split(None, 1)[0].rstrip(")")


def remove_nested_blocks(block, head):
    lines = block.splitlines(keepends=True)
    out = []
    i = 0
    while i < len(lines):
        stripped = lines[i].lstrip()
        if stripped.startswith("(" + head):
            depth = 0
            while i < len(lines):
                depth += paren_delta(lines[i])
                i += 1
                if depth == 0:
                    break
            continue
        out.append(lines[i])
        i += 1
    return "".join(out)


def replace_polygon(block, points):
    lines = block.splitlines(keepends=True)
    out = []
    i = 0
    replacement = "\t\t(polygon\n\t\t\t(pts\n"
    rows = []
    for x_mm, y_mm in points:
        rows.append(f"(xy {x_mm:g} {y_mm:g})")
    replacement += "\t\t\t\t" + " ".join(rows[:4]) + "\n"
    if len(rows) > 4:
        replacement += "\t\t\t\t" + " ".join(rows[4:]) + "\n"
    replacement += "\t\t\t)\n\t\t)\n"
    while i < len(lines):
        stripped = lines[i].lstrip()
        if stripped.startswith("(polygon"):
            depth = 0
            while i < len(lines):
                depth += paren_delta(lines[i])
                i += 1
                if depth == 0:
                    break
            out.append(replacement)
            continue
        out.append(lines[i])
        i += 1
    return "".join(out)


def gr_line_block(x1, y1, x2, y2):
    return (
        "\t(gr_line\n"
        f"\t\t(start {x1:g} {y1:g})\n"
        f"\t\t(end {x2:g} {y2:g})\n"
        '\t\t(stroke\n\t\t\t(width 0.1)\n\t\t\t(type solid)\n\t\t)\n'
        '\t\t(layer "Edge.Cuts")\n'
        f'\t\t(uuid "{uuid.uuid4()}")\n'
        "\t)\n"
    )


def gr_text_block(text, x, y, layer, angle=0, size=0.7, thickness=0.1, mirror=False, justify=None):
    justify_bits = []
    if justify:
        justify_bits.append(justify)
    if mirror:
        justify_bits.append("mirror")
    justify_block = ""
    if justify_bits:
        justify_block = "\t\t\t(justify " + " ".join(justify_bits) + ")\n"
    return (
        f'\t(gr_text "{text}"\n'
        f"\t\t(at {x:g} {y:g} {angle:g})\n"
        f'\t\t(layer "{layer}")\n'
        f'\t\t(uuid "{uuid.uuid4()}")\n'
        "\t\t(effects\n"
        "\t\t\t(font\n"
        f"\t\t\t\t(size {size:g} {size:g})\n"
        f"\t\t\t\t(thickness {thickness:g})\n"
        "\t\t\t)\n"
        f"{justify_block}"
        "\t\t)\n"
        "\t)\n"
    )


def keepout_zone_block(name, layer, x0, y0, x1, y1):
    return (
        "\t(zone\n"
        f'\t\t(layer "{layer}")\n'
        f'\t\t(uuid "{uuid.uuid4()}")\n'
        f'\t\t(name "{name}")\n'
        "\t\t(hatch edge 0.5)\n"
        "\t\t(connect_pads\n"
        "\t\t\t(clearance 0)\n"
        "\t\t)\n"
        "\t\t(min_thickness 0.25)\n"
        "\t\t(keepout\n"
        "\t\t\t(tracks not_allowed)\n"
        "\t\t\t(vias not_allowed)\n"
        "\t\t\t(pads allowed)\n"
        "\t\t\t(copperpour not_allowed)\n"
        "\t\t\t(footprints allowed)\n"
        "\t\t)\n"
        "\t\t(placement\n"
        "\t\t\t(enabled no)\n"
        '\t\t\t(sheetname "")\n'
        "\t\t)\n"
        "\t\t(fill\n"
        "\t\t\t(thermal_gap 0.5)\n"
        "\t\t\t(thermal_bridge_width 0.5)\n"
        "\t\t\t(island_removal_mode 0)\n"
        "\t\t)\n"
        "\t\t(polygon\n"
        "\t\t\t(pts\n"
        f"\t\t\t\t(xy {x0:g} {y0:g}) (xy {x1:g} {y0:g}) (xy {x1:g} {y1:g}) (xy {x0:g} {y1:g})\n"
        "\t\t\t)\n"
        "\t\t)\n"
        "\t)\n"
    )


def generated_mounting_keepouts(config):
    blocks = []
    for ref in ("MH1", "MH3"):
        if ref not in config["moves"]:
            continue
        x, y, _angle = config["moves"][ref]
        x0, y0, x1, y1 = x - 3.0, y - 3.0, x + 3.0, y + 3.0
        blocks.append(keepout_zone_block(f"{ref}_ROUTE_KEEPOUT_FRONT", "F.Cu", x0, y0, x1, y1))
        blocks.append(keepout_zone_block(f"{ref}_ROUTE_KEEPOUT_BACK", "B.Cu", x0, y0, x1, y1))
    return "".join(blocks)


def via_block(x, y, net, size=0.6, drill=0.3, keep_uncovered=False):
    remove_unused = "\t\t(remove_unused_layers no)\n" if keep_uncovered else ""
    keepout = "\t\t(keep_end_layers)\n" if keep_uncovered else ""
    return (
        "\t(via\n"
        f"\t\t(at {x:g} {y:g})\n"
        f"\t\t(size {size:g})\n"
        f"\t\t(drill {drill:g})\n"
        '\t\t(layers "F.Cu" "B.Cu")\n'
        f'\t\t(net "{net}")\n'
        f"{remove_unused}"
        f"{keepout}"
        f'\t\t(uuid "{uuid.uuid4()}")\n'
        "\t)\n"
    )


def segment_block(x1, y1, x2, y2, net, layer="B.Cu", width=0.25):
    return (
        "\t(segment\n"
        f"\t\t(start {x1:g} {y1:g})\n"
        f"\t\t(end {x2:g} {y2:g})\n"
        f"\t\t(width {width:g})\n"
        f'\t\t(layer "{layer}")\n'
        f'\t\t(net "{net}")\n'
        f'\t\t(uuid "{uuid.uuid4()}")\n'
        "\t)\n"
    )


def generated_graphics(config):
    x0, y0, x1, y1 = config["bounds"]
    blocks = [
        gr_line_block(x0, y0, x1, y0),
        gr_line_block(x1, y0, x1, y1),
        gr_line_block(x1, y1, x0, y1),
        gr_line_block(x0, y1, x0, y0),
        gr_text_block("ROADPING C3 EVT2", 42.0, y1 - 0.75, "F.SilkS", 0, 0.65, 0.1),
        gr_text_block("OLED", 42.7, 32.55, "F.SilkS", 0, 0.65, 0.1),
        gr_text_block("3V3", 42.7, 34.0, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("GND", 42.7, 36.54, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("SCL", 42.7, 39.08, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("SDA", 42.7, 41.62, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("USB", 26.0, 13.85, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("GND", 26.0, 18.25, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("BAT+", 56.7, 15.25, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("BAT-", 56.7, 18.25, "F.SilkS", 0, 0.62, 0.1),
        gr_text_block("D1K", 31.2, 21.7, "F.SilkS", 0, 0.58, 0.09),
    ]
    for _ref, (tx, ty, _angle, label) in config["testpads"].items():
        blocks.append(gr_text_block(label, tx, ty + 1.45, "F.SilkS", 0, 0.58, 0.09))
    boot_x, boot_y = config["boot"]
    blocks.append(gr_text_block("BOOT", boot_x + 1.7, boot_y, "F.SilkS", 0, 0.62, 0.1, False, "left"))
    f1 = config["moves"]["F1"]
    f2 = config["moves"]["F2"]
    blocks.append(gr_text_block("F1 USB", f1[0] - 2.6, f1[1], "B.SilkS", 90, 0.62, 0.1, True))
    blocks.append(gr_text_block("F2 BAT", f2[0], f2[1] + 2.8, "B.SilkS", 0, 0.62, 0.1, True))
    return "".join(blocks)


def generated_seed_routes(config):
    blocks = []
    boot_x, boot_y = config["boot"]
    blocks.append(via_block(boot_x, boot_y, "unconnected-(U3-GPIO9-Pad9)", 1.2, 0.5, True))
    blocks.append(segment_block(24.24, 35.88, boot_x, boot_y, "unconnected-(U3-GPIO9-Pad9)", "B.Cu", 0.25))
    for x_mm, y_mm in config["gnd_stitches"]:
        blocks.append(via_block(x_mm, y_mm, "/GND", 0.6, 0.3, False))
    return "".join(blocks)


DELETE_FOOTPRINT_REFS = {"MH2", "MH4"}


def cleanup_sexpr(source, destination, config_name, clear_routes=True):
    config = ITERATIONS[config_name]
    source_text = Path(source).read_text()
    lines = source_text.splitlines(keepends=True)
    out = []
    inserted_graphics = False
    inserted_routes = False
    removed = {"edge_cuts": 0, "gr_text": 0, "segments": 0, "vias": 0}
    i = 0
    x0, y0, x1, y1 = config["bounds"]
    kx0, ky0, kx1, ky1 = config["keepout"]
    gnd_points = [
        (x0 + 0.5, y0 + 0.42),
        (x1 - 0.5, y0 + 0.42),
        (x1 - 0.5, y1 - 0.2),
        (x0 + 0.5, y1 - 0.2),
        (x0 + 0.5, y1 - 0.2),
        (kx1, ky1),
        (kx1, ky0),
        (kx0, ky0),
    ]

    while i < len(lines):
        line = lines[i]
        head = top_level_head(line)
        if head in {"gr_line", "gr_text", "segment", "via", "zone", "footprint"}:
            depth = 0
            block_lines = []
            while i < len(lines):
                block_lines.append(lines[i])
                depth += paren_delta(lines[i])
                i += 1
                if depth == 0:
                    break
            block = "".join(block_lines)
            if head == "footprint":
                for ref in DELETE_FOOTPRINT_REFS:
                    if f'(property "Reference" "{ref}"' in block:
                        break
                else:
                    out.append(block)
                    continue
                continue
            if head == "gr_line" and '(layer "Edge.Cuts")' in block:
                removed["edge_cuts"] += 1
                continue
            if head == "gr_text":
                removed["gr_text"] += 1
                continue
            if clear_routes and head == "segment":
                removed["segments"] += 1
                continue
            if clear_routes and head == "via":
                removed["vias"] += 1
                continue
            if head == "zone":
                if not inserted_graphics:
                    out.append(generated_graphics(config))
                    out.append(generated_mounting_keepouts(config))
                    inserted_graphics = True
                if clear_routes and not inserted_routes:
                    out.append(generated_seed_routes(config))
                    inserted_routes = True
                if "ROUTE_KEEPOUT_" in block:
                    continue
                if 'name "EVT_V2_GND_ZONE_FRONT"' in block or 'name "EVT_V2_GND_ZONE_BACK"' in block:
                    block = remove_nested_blocks(block, "filled_polygon")
                    block = replace_polygon(block, gnd_points)
                out.append(block)
                continue
            out.append(block)
            continue
        out.append(line)
        i += 1

    Path(destination).write_text("".join(out))
    print(json.dumps({"source": str(source), "destination": str(destination), "iteration": config_name, "clear_routes": clear_routes, "removed": removed}, indent=2))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    p_apply = sub.add_parser("apply")
    p_apply.add_argument("source", type=Path)
    p_apply.add_argument("destination", type=Path)
    p_apply.add_argument("--iteration", choices=sorted(ITERATIONS), required=True)
    p_apply.add_argument("--keep-routes", action="store_true")

    p_export = sub.add_parser("export-dsn")
    p_export.add_argument("board", type=Path)
    p_export.add_argument("dsn", type=Path)

    p_import = sub.add_parser("import-ses")
    p_import.add_argument("board", type=Path)
    p_import.add_argument("ses", type=Path)
    p_import.add_argument("output", type=Path)

    p_widths = sub.add_parser("set-widths")
    p_widths.add_argument("board", type=Path)
    p_widths.add_argument("output", type=Path)

    p_inspect = sub.add_parser("inspect")
    p_inspect.add_argument("board", type=Path)

    p_cleanup = sub.add_parser("cleanup-sexpr")
    p_cleanup.add_argument("source", type=Path)
    p_cleanup.add_argument("destination", type=Path)
    p_cleanup.add_argument("--iteration", choices=sorted(ITERATIONS), required=True)
    p_cleanup.add_argument("--keep-routes", action="store_true")

    args = parser.parse_args()
    if args.command == "apply":
        apply_config(args.source, args.destination, args.iteration, not args.keep_routes)
    elif args.command == "export-dsn":
        export_dsn(args.board, args.dsn)
    elif args.command == "import-ses":
        import_ses(args.board, args.ses, args.output)
    elif args.command == "set-widths":
        set_widths(args.board, args.output)
    elif args.command == "inspect":
        inspect(args.board)
    elif args.command == "cleanup-sexpr":
        cleanup_sexpr(args.source, args.destination, args.iteration, not args.keep_routes)


if __name__ == "__main__":
    main()
