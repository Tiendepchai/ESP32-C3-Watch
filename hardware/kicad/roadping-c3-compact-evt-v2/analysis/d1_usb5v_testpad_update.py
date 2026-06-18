#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import pcbnew


def mm(value):
    return pcbnew.FromMM(value)


def point(x_mm, y_mm):
    return pcbnew.VECTOR2I(mm(x_mm), mm(y_mm))


def fp_map(board):
    return {fp.GetReference(): fp for fp in board.GetFootprints()}


def set_footprint(fp, x_mm, y_mm, angle_deg):
    fp.SetPosition(point(x_mm, y_mm))
    fp.SetOrientationDegrees(angle_deg)


def ensure_side(fp, target_layer):
    if fp.GetLayer() == target_layer:
        return False
    fp.Flip(fp.GetPosition(), False)
    if fp.GetLayer() != target_layer:
        fp.Flip(fp.GetPosition(), True)
    if fp.GetLayer() != target_layer:
        raise RuntimeError(f"Could not move {fp.GetReference()} to target layer")
    return True


def remove_footprint(board, reference):
    for fp in list(board.GetFootprints()):
        if fp.GetReference() == reference:
            board.Remove(fp)


def layer_set(*layers):
    layerset = pcbnew.LSET()
    for layer in layers:
        layerset.AddLayer(layer)
    return layerset


def add_usb5v_in_testpad(board, existing=None):
    net = board.FindNet("/USB_5V_IN")
    if net is None:
        raise RuntimeError("Missing /USB_5V_IN net")

    fp = existing
    if fp is None:
        fp = pcbnew.FOOTPRINT(board)
        fp.SetFPID(pcbnew.LIB_ID("roadping-c3", "TP_Round_1.0mm"))
        fp.SetReference("TP16")
        fp.SetValue("TP_USB5V")
    else:
        fp.SetValue("TP_USB5V")
    fp.SetLayer(pcbnew.B_Cu)
    fp.SetPosition(point(48.4, 22.0))
    fp.SetAttributes(
        pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES
    )

    pads = list(fp.Pads())
    pad = pads[0] if pads else pcbnew.PAD(fp)
    pad.SetNumber("1")
    pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(point(1.3, 1.3))
    pad.SetLayerSet(layer_set(pcbnew.B_Cu, pcbnew.B_Mask))
    pad.SetPosition(point(48.4, 22.0))
    pad.SetNet(net)
    if not pads:
        fp.Add(pad)
    if existing is None:
        board.Add(fp)


def add_text(board, text_value, x_mm, y_mm, layer, angle=0.0, size=0.65, mirror=False):
    text = pcbnew.PCB_TEXT(board)
    text.SetText(text_value)
    text.SetPosition(point(x_mm, y_mm))
    text.SetLayer(layer)
    text.SetTextSize(point(size, size))
    text.SetTextThickness(mm(0.1))
    text.SetTextAngleDegrees(angle)
    text.SetMirrored(mirror)
    board.Add(text)


def remove_our_text(board):
    remove_text = {"USB EDGE", "USB", "5V", "GND", "5V_IN", "D1 TVS", "F1 USB", "BAT+", "BAT-"}
    for drawing in list(board.GetDrawings()):
        if isinstance(drawing, pcbnew.PCB_TEXT) and drawing.GetText() in remove_text:
            pos = drawing.GetPosition()
            x = pcbnew.ToMM(pos.x)
            y = pcbnew.ToMM(pos.y)
            if (15.0 <= x <= 56.5 and 12.5 <= y <= 24.0) or (
                drawing.GetText() == "F1 USB" and 15.0 <= x <= 50.0 and 18.0 <= y <= 37.0
            ):
                board.Remove(drawing)


def add_usb_cluster_labels(board):
    remove_our_text(board)
    add_text(board, "USB EDGE", 45.6, 13.35, pcbnew.F_SilkS, 0, 0.6)
    add_text(board, "5V", 45.65, 16.45, pcbnew.F_SilkS, 0, 0.6)
    add_text(board, "GND", 48.35, 16.45, pcbnew.F_SilkS, 0, 0.6)
    add_text(board, "BAT+", 54.45, 19.7, pcbnew.F_SilkS, 0, 0.6)
    add_text(board, "BAT-", 54.45, 24.3, pcbnew.F_SilkS, 0, 0.6)
    add_text(board, "5V_IN", 49.2, 25.1, pcbnew.B_SilkS, 0, 0.6, True)
    add_text(board, "D1 TVS", 47.8, 20.55, pcbnew.B_SilkS, 0, 0.55, True)
    add_text(board, "F1 USB", 45.4, 24.4, pcbnew.B_SilkS, 0, 0.55, True)


def add_via(board, x_mm, y_mm, net_name="/GND", size_mm=0.6, drill_mm=0.3):
    net = board.FindNet(net_name)
    if net is None:
        raise RuntimeError(f"Missing net {net_name}")
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(x_mm, y_mm))
    via.SetWidth(mm(size_mm))
    via.SetDrill(mm(drill_mm))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    board.Add(via)


def remove_vias_near(board, points, tolerance_mm=0.08):
    for item in list(board.GetTracks()):
        if not isinstance(item, pcbnew.PCB_VIA):
            continue
        pos = item.GetPosition()
        x = pcbnew.ToMM(pos.x)
        y = pcbnew.ToMM(pos.y)
        for x0, y0 in points:
            if abs(x - x0) <= tolerance_mm and abs(y - y0) <= tolerance_mm:
                board.Remove(item)
                break


def add_gnd_stitches(board):
    stale_or_managed = [
        (43.4, 18.4),
        (47.3, 20.4),
        (17.8, 29.0),
        (30.0, 42.4),
        (39.4, 36.6),
        (50.0, 42.8),
        (55.6, 48.4),
        (43.0, 17.3),
        (33.2, 42.4),
        (16.5, 40.6),
        (34.2, 30.8),
        (24.2, 37.2),
        (29.2, 44.3),
        (23.6, 43.3),
        (35.3, 46.9),
        (44.4, 52.4),
    ]
    remove_vias_near(board, stale_or_managed)
    stitches = [
        (43.0, 17.3),  # Type-C/breakout GND return
        (47.3, 20.4),  # D1 TVS clamp return
        (17.8, 29.0),  # TP4056 IN-/module return
        (33.2, 42.4),  # regulator/ESP32 return
        (50.0, 42.8),  # encoder return
        (55.6, 48.4),  # debug/test-pad return
        (16.5, 40.6),  # ESP32/support ground island tie
        (34.2, 30.8),  # regulator ground island tie
        (24.2, 37.2),  # OLED ground island tie
        (29.2, 44.3),  # C2 ground island tie
        (23.6, 43.3),  # BAT_ADC divider/filter ground island tie
        (35.3, 46.9),  # C3 ground island tie
        (44.4, 52.4),  # J3 pin 6 ground tie
    ]
    for x_mm, y_mm in stitches:
        add_via(board, x_mm, y_mm)


def apply_update(source, destination):
    board = pcbnew.LoadBoard(str(source))
    footprints = fp_map(board)
    add_usb_cluster_labels(board)

    # J1 is the board-edge 5 V/GND input for an external USB-C 5 V breakout.
    set_footprint(footprints["J1"], 46.1, 14.65, 90.0)

    # Keep the battery connector accessible after moving J1 to the top edge.
    set_footprint(footprints["J2"], 52.4, 21.0, -90.0)

    # F1 remains the series PTC: /USB_5V_IN -> /USB_5V.
    set_footprint(footprints["F1"], 44.4, 20.7, 90.0)

    # D1 is a TVS clamp, not a series diode. Pad 1 remains GND, pad 2 remains /USB_5V_IN.
    d1 = footprints["D1"]
    moved_to_back = ensure_side(d1, pcbnew.B_Cu)
    set_footprint(d1, 47.4, 18.5, -90.0)

    add_gnd_stitches(board)
    add_usb5v_in_testpad(board, footprints.get("TP16"))

    pcbnew.SaveBoard(str(destination), board)
    print(
        json.dumps(
            {
                "source": str(source),
                "destination": str(destination),
                "d1_moved_to_back": moved_to_back or d1.GetLayer() == pcbnew.B_Cu,
                "placements": {
                    "J1": {"x": 46.1, "y": 14.65, "angle": 90.0, "function": "edge 5V/GND input"},
                    "J2": {"x": 52.4, "y": 21.0, "angle": -90.0},
                    "F1": {"x": 44.4, "y": 20.7, "angle": 90.0},
                    "D1": {"x": 47.4, "y": 18.5, "angle": -90.0, "layer": "B.Cu"},
                    "TP16": {"x": 48.4, "y": 22.0, "layer": "B.Cu", "net": "/USB_5V_IN"},
                },
            },
            indent=2,
        )
    )


def inspect(board_path):
    board = pcbnew.LoadBoard(str(board_path))
    data = {}
    for ref in ("J1", "J2", "D1", "F1", "TP16", "U1"):
        fp = fp_map(board).get(ref)
        if not fp:
            continue
        data[ref] = {
            "layer": board.GetLayerName(fp.GetLayer()),
            "x": round(pcbnew.ToMM(fp.GetPosition().x), 3),
            "y": round(pcbnew.ToMM(fp.GetPosition().y), 3),
            "angle": round(fp.GetOrientationDegrees(), 3),
            "pads": {
                pad.GetNumber(): {
                    "net": pad.GetNetname(),
                    "x": round(pcbnew.ToMM(pad.GetPosition().x), 3),
                    "y": round(pcbnew.ToMM(pad.GetPosition().y), 3),
                    "layers": [board.GetLayerName(layer) for layer in pad.GetLayerSet().Seq()],
                }
                for pad in fp.Pads()
            },
        }
    print(json.dumps(data, indent=2))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    p_apply = sub.add_parser("apply")
    p_apply.add_argument("source", type=Path)
    p_apply.add_argument("destination", type=Path)

    p_inspect = sub.add_parser("inspect")
    p_inspect.add_argument("board", type=Path)

    p_labels = sub.add_parser("labels")
    p_labels.add_argument("board", type=Path)

    args = parser.parse_args()
    if args.command == "apply":
        apply_update(args.source, args.destination)
    elif args.command == "inspect":
        inspect(args.board)
    elif args.command == "labels":
        board = pcbnew.LoadBoard(str(args.board))
        add_usb_cluster_labels(board)
        pcbnew.SaveBoard(str(args.board), board)


if __name__ == "__main__":
    main()
