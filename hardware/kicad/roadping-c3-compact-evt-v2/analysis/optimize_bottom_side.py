#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import pcbnew


AFFECTED_POWER_NETS = {
    "/USB_5V_IN",
    "/USB_5V",
    "/BAT_CONN+",
    "/VBAT",
}

U3_REROUTE_NETS = {
    "/3V3",
    "/BAT_ADC",
    "/ENC_A",
    "/ENC_B",
    "/ENC_SW",
    "/I2C_SCL",
    "/I2C_SDA",
    "/UART_RX",
    "/UART_TX",
    "unconnected-(U3-GPIO9-Pad9)",
}


def mm(value):
    return pcbnew.FromMM(value)


def point(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def footprint_map(board):
    return {fp.GetReference(): fp for fp in board.GetFootprints()}


def net_name(item):
    net = item.GetNet()
    return net.GetNetname() if net else ""


def summary(board):
    footprints = list(board.GetFootprints())
    tracks = list(board.GetTracks())
    return {
        "footprints": len(footprints),
        "top_footprints": sum(1 for fp in footprints if fp.GetLayer() == pcbnew.F_Cu),
        "bottom_footprints": sum(1 for fp in footprints if fp.GetLayer() == pcbnew.B_Cu),
        "track_segments": sum(
            1 for item in tracks if not isinstance(item, pcbnew.PCB_VIA)
        ),
        "vias": sum(1 for item in tracks if isinstance(item, pcbnew.PCB_VIA)),
        "zones": board.GetAreaCount(),
    }


def connected_items(board, selected_nets):
    rows = []
    for item in board.GetTracks():
        name = net_name(item)
        if name not in selected_nets:
            continue
        row = {
            "kind": "via" if isinstance(item, pcbnew.PCB_VIA) else "track",
            "net": name,
            "start_mm": [
                round(pcbnew.ToMM(item.GetStart().x), 4),
                round(pcbnew.ToMM(item.GetStart().y), 4),
            ],
            "end_mm": [
                round(pcbnew.ToMM(item.GetEnd().x), 4),
                round(pcbnew.ToMM(item.GetEnd().y), 4),
            ],
            "width_mm": round(pcbnew.ToMM(item.GetWidth()), 4),
            "layer": board.GetLayerName(item.GetLayer()),
        }
        rows.append(row)
    return rows


def inspect(board_path, selected_nets):
    board = pcbnew.LoadBoard(str(board_path))
    fps = footprint_map(board)
    data = {
        "summary": summary(board),
        "footprints": {},
        "tracks": connected_items(board, selected_nets),
    }
    for ref in ("F1", "F2", "D1", "U1", "U2", "U3", "J1", "J2", "J3"):
        fp = fps.get(ref)
        if not fp:
            continue
        data["footprints"][ref] = {
            "layer": board.GetLayerName(fp.GetLayer()),
            "position_mm": [
                round(pcbnew.ToMM(fp.GetPosition().x), 4),
                round(pcbnew.ToMM(fp.GetPosition().y), 4),
            ],
            "orientation_deg": round(fp.GetOrientationDegrees(), 3),
            "pads": [
                {
                    "number": pad.GetNumber(),
                    "net": pad.GetNetname(),
                    "position_mm": [
                        round(pcbnew.ToMM(pad.GetPosition().x), 4),
                        round(pcbnew.ToMM(pad.GetPosition().y), 4),
                    ],
                }
                for pad in fp.Pads()
            ],
        }
    print(json.dumps(data, indent=2))


def remove_selected_routes(board, selected_nets):
    removed = []
    for item in list(board.GetTracks()):
        name = net_name(item)
        if name not in selected_nets:
            continue
        removed.append(
            {
                "kind": "via" if isinstance(item, pcbnew.PCB_VIA) else "track",
                "net": name,
            }
        )
        board.Remove(item)
    return removed


def flip_in_place(fp, flip_left_right):
    original_position = fp.GetPosition()
    fp.Flip(original_position, flip_left_right)
    fp.SetPosition(original_position)


def add_boot_probe(board, x_mm, y_mm):
    fps = footprint_map(board)
    u3 = fps["U3"]
    gpio9 = next(pad for pad in u3.Pads() if pad.GetNumber() == "9")
    target = point(x_mm, y_mm)

    via = pcbnew.PCB_VIA(board)
    via.SetPosition(target)
    via.SetWidth(mm(1.2))
    via.SetDrill(mm(0.5))
    via.SetNet(gpio9.GetNet())
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    if hasattr(via, "SetFrontTenting"):
        via.SetFrontTenting(False)
        via.SetBackTenting(False)
    board.Add(via)

    track = pcbnew.PCB_TRACK(board)
    track.SetStart(gpio9.GetPosition())
    track.SetEnd(target)
    track.SetWidth(mm(0.25))
    track.SetLayer(pcbnew.B_Cu)
    track.SetNet(gpio9.GetNet())
    board.Add(track)

    text = pcbnew.PCB_TEXT(board)
    text.SetText("BOOT")
    text.SetPosition(point(x_mm + 2.4, y_mm))
    text.SetLayer(pcbnew.F_SilkS)
    text.SetTextSize(point(1.0, 1.0))
    text.SetTextThickness(mm(0.15))
    text.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    board.Add(text)


def add_usb_input_transition(board):
    fps = footprint_map(board)
    f1_pad = next(pad for pad in fps["F1"].Pads() if pad.GetNumber() == "1")
    target = point(50.8, 19.4)

    via = pcbnew.PCB_VIA(board)
    via.SetPosition(target)
    via.SetWidth(mm(0.8))
    via.SetDrill(mm(0.4))
    via.SetNet(f1_pad.GetNet())
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    board.Add(via)

    track = pcbnew.PCB_TRACK(board)
    track.SetStart(f1_pad.GetPosition())
    track.SetEnd(target)
    track.SetWidth(mm(0.25))
    track.SetLayer(pcbnew.B_Cu)
    track.SetNet(f1_pad.GetNet())
    board.Add(track)


def make_manual_candidate(source, destination, add_boot, boot_x, boot_y):
    board = pcbnew.LoadBoard(str(source))
    fps = footprint_map(board)
    before = summary(board)

    flip_in_place(fps["F1"], False)
    flip_in_place(fps["F2"], True)

    moved_tracks = 0
    for item in board.GetTracks():
        if isinstance(item, pcbnew.PCB_VIA):
            continue
        if net_name(item) in {"/USB_5V", "/BAT_CONN+", "/VBAT"}:
            item.SetLayer(pcbnew.B_Cu)
            moved_tracks += 1

    add_usb_input_transition(board)
    if add_boot:
        add_boot_probe(board, boot_x, boot_y)

    pcbnew.SaveBoard(str(destination), board)
    print(
        json.dumps(
            {
                "source": str(source),
                "destination": str(destination),
                "before": before,
                "after": summary(board),
                "moved_tracks_to_b_cu": moved_tracks,
                "boot_probe": [boot_x, boot_y] if add_boot else None,
            },
            indent=2,
        )
    )


def make_candidate(source, destination, add_boot, boot_x, boot_y):
    board = pcbnew.LoadBoard(str(source))
    fps = footprint_map(board)
    before = summary(board)

    flip_in_place(fps["F1"], False)
    flip_in_place(fps["F2"], True)

    if add_boot:
        add_boot_probe(board, boot_x, boot_y)

    removed = remove_selected_routes(board, AFFECTED_POWER_NETS)

    after = dict(before)
    removed_track_count = sum(1 for item in removed if item["kind"] == "track")
    removed_via_count = sum(1 for item in removed if item["kind"] == "via")
    after["top_footprints"] -= 2
    after["bottom_footprints"] += 2
    after["track_segments"] += (1 if add_boot else 0) - removed_track_count
    after["vias"] += (1 if add_boot else 0) - removed_via_count

    pcbnew.SaveBoard(str(destination), board)
    result = {
        "source": str(source),
        "destination": str(destination),
        "before": before,
        "after": after,
        "removed_routes": len(removed),
        "removed_by_net": {
            name: sum(1 for item in removed if item["net"] == name)
            for name in sorted(AFFECTED_POWER_NETS)
        },
        "boot_probe": [boot_x, boot_y] if add_boot else None,
    }
    print(json.dumps(result, indent=2))


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
    print(
        json.dumps(
            {
                "board": str(board_path),
                "ses": str(ses_path),
                "output": str(output_path),
                "summary": summary(board),
            },
            indent=2,
        )
    )


def set_track_width(
    board_path,
    output_path,
    selected_nets,
    width_mm,
    narrow_uuids=None,
    narrow_width_mm=None,
):
    board = pcbnew.LoadBoard(str(board_path))
    changed = 0
    narrowed = 0
    narrow_uuids = narrow_uuids or set()
    for item in board.GetTracks():
        if isinstance(item, pcbnew.PCB_VIA):
            continue
        if net_name(item) in selected_nets:
            item.SetWidth(mm(width_mm))
            changed += 1
        if item.m_Uuid.AsString() in narrow_uuids:
            item.SetWidth(mm(narrow_width_mm))
            narrowed += 1
    pcbnew.SaveBoard(str(output_path), board)
    print(
        json.dumps(
            {
                "board": str(board_path),
                "output": str(output_path),
                "nets": sorted(selected_nets),
                "width_mm": width_mm,
                "changed_tracks": changed,
                "narrow_width_mm": narrow_width_mm,
                "narrowed_tracks": narrowed,
                "summary": summary(board),
            },
            indent=2,
        )
    )


def transplant_routes(target_path, route_source_path, output_path, selected_nets):
    target = pcbnew.LoadBoard(str(target_path))
    route_source = pcbnew.LoadBoard(str(route_source_path))
    added_tracks = 0
    added_vias = 0

    for item in route_source.GetTracks():
        name = net_name(item)
        if name not in selected_nets:
            continue
        net = target.FindNet(name)
        if isinstance(item, pcbnew.PCB_VIA):
            clone = pcbnew.PCB_VIA(target)
            clone.SetPosition(item.GetPosition())
            clone.SetWidth(item.GetWidth(pcbnew.F_Cu))
            clone.SetDrill(item.GetDrillValue())
            clone.SetLayerPair(item.TopLayer(), item.BottomLayer())
            clone.SetNet(net)
            target.Add(clone)
            added_vias += 1
        else:
            clone = pcbnew.PCB_TRACK(target)
            clone.SetStart(item.GetStart())
            clone.SetEnd(item.GetEnd())
            clone.SetWidth(item.GetWidth())
            clone.SetLayer(item.GetLayer())
            clone.SetNet(net)
            target.Add(clone)
            added_tracks += 1

    pcbnew.SaveBoard(str(output_path), target)
    print(
        json.dumps(
            {
                "target": str(target_path),
                "route_source": str(route_source_path),
                "output": str(output_path),
                "nets": sorted(selected_nets),
                "added_tracks": added_tracks,
                "added_vias": added_vias,
                "summary": summary(target),
            },
            indent=2,
        )
    )


def add_bottom_labels(board_path, output_path):
    board = pcbnew.LoadBoard(str(board_path))
    labels = [
        ("F1 USB", 47.4, 18.0, 90.0),
        ("F2 BAT", 50.7, 25.6, 0.0),
    ]
    for content, x_mm, y_mm, angle_deg in labels:
        text = pcbnew.PCB_TEXT(board)
        text.SetText(content)
        text.SetPosition(point(x_mm, y_mm))
        text.SetLayer(pcbnew.B_SilkS)
        text.SetMirrored(True)
        text.SetTextAngleDegrees(angle_deg)
        text.SetTextSize(point(0.9, 0.9))
        text.SetTextThickness(mm(0.14))
        board.Add(text)
    pcbnew.SaveBoard(str(output_path), board)
    print(
        json.dumps(
            {
                "board": str(board_path),
                "output": str(output_path),
                "labels": [item[0] for item in labels],
                "summary": summary(board),
            },
            indent=2,
        )
    )


def add_ground_via(board_path, output_path, x_mm, y_mm):
    board = pcbnew.LoadBoard(str(board_path))
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(x_mm, y_mm))
    via.SetWidth(mm(0.8))
    via.SetDrill(mm(0.4))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(board.FindNet("/GND"))
    board.Add(via)
    pcbnew.SaveBoard(str(output_path), board)
    print(
        json.dumps(
            {
                "board": str(board_path),
                "output": str(output_path),
                "ground_via_mm": [x_mm, y_mm],
                "summary": summary(board),
            },
            indent=2,
        )
    )


def add_antenna_keepout(board, layer):
    zone = pcbnew.ZONE(board)
    zone.SetZoneName(
        "U3_ANTENNA_KEEPOUT_" + ("FRONT" if layer == pcbnew.F_Cu else "BACK")
    )
    zone.SetLayer(layer)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowPads(True)
    zone.SetDoNotAllowZoneFills(True)
    zone.SetDoNotAllowFootprints(False)
    outline = zone.Outline()
    outline.NewOutline()
    for x_mm, y_mm in (
        (13.1, 33.2),
        (15.7, 33.2),
        (15.7, 53.5),
        (13.1, 53.5),
    ):
        outline.Append(point(x_mm, y_mm))
    board.Add(zone)


def make_antenna_corrected_candidate(source, destination):
    board = pcbnew.LoadBoard(str(source))
    fps = footprint_map(board)
    before = summary(board)

    u3 = fps["U3"]
    u3.SetOrientationDegrees(u3.GetOrientationDegrees() + 180.0)

    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        add_antenna_keepout(board, layer)

    for drawing in list(board.GetDrawings()):
        if isinstance(drawing, pcbnew.PCB_TEXT) and drawing.GetText() == "BOOT":
            board.Remove(drawing)

    removed = remove_selected_routes(board, U3_REROUTE_NETS)
    pcbnew.SaveBoard(str(destination), board)
    print(
        json.dumps(
            {
                "source": str(source),
                "destination": str(destination),
                "before": before,
                "removed_routes": len(removed),
                "removed_by_net": {
                    name: sum(1 for item in removed if item["net"] == name)
                    for name in sorted(U3_REROUTE_NETS)
                },
                "u3_orientation_deg": u3.GetOrientationDegrees(),
                "keepout_mm": [13.1, 33.2, 15.7, 53.5],
            },
            indent=2,
        )
    )


def formalize_antenna_keepout(board_path, output_path):
    board = pcbnew.LoadBoard(str(board_path))
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        add_antenna_keepout(board, layer)
    pcbnew.SaveBoard(str(output_path), board)
    print(
        json.dumps(
            {
                "board": str(board_path),
                "output": str(output_path),
                "keepout_mm": [13.1, 33.2, 15.7, 53.5],
                "summary": summary(board),
            },
            indent=2,
        )
    )


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    inspect_parser = sub.add_parser("inspect")
    inspect_parser.add_argument("board", type=Path)
    inspect_parser.add_argument(
        "--nets",
        nargs="*",
        default=sorted(AFFECTED_POWER_NETS),
    )

    candidate_parser = sub.add_parser("make-candidate")
    candidate_parser.add_argument("source", type=Path)
    candidate_parser.add_argument("destination", type=Path)
    candidate_parser.add_argument("--add-boot", action="store_true")
    candidate_parser.add_argument("--boot-x", type=float, default=36.8)
    candidate_parser.add_argument("--boot-y", type=float, default=35.9)

    manual_parser = sub.add_parser("make-manual-candidate")
    manual_parser.add_argument("source", type=Path)
    manual_parser.add_argument("destination", type=Path)
    manual_parser.add_argument("--add-boot", action="store_true")
    manual_parser.add_argument("--boot-x", type=float, default=24.24)
    manual_parser.add_argument("--boot-y", type=float, default=32.7)

    export_parser = sub.add_parser("export-dsn")
    export_parser.add_argument("board", type=Path)
    export_parser.add_argument("dsn", type=Path)

    import_parser = sub.add_parser("import-ses")
    import_parser.add_argument("board", type=Path)
    import_parser.add_argument("ses", type=Path)
    import_parser.add_argument("output", type=Path)

    width_parser = sub.add_parser("set-width")
    width_parser.add_argument("board", type=Path)
    width_parser.add_argument("output", type=Path)
    width_parser.add_argument("--width", type=float, required=True)
    width_parser.add_argument("--nets", nargs="+", required=True)
    width_parser.add_argument("--narrow-uuid", nargs="*", default=[])
    width_parser.add_argument("--narrow-width", type=float)

    transplant_parser = sub.add_parser("transplant-routes")
    transplant_parser.add_argument("target", type=Path)
    transplant_parser.add_argument("route_source", type=Path)
    transplant_parser.add_argument("output", type=Path)
    transplant_parser.add_argument("--nets", nargs="+", required=True)

    label_parser = sub.add_parser("add-bottom-labels")
    label_parser.add_argument("board", type=Path)
    label_parser.add_argument("output", type=Path)

    gnd_via_parser = sub.add_parser("add-ground-via")
    gnd_via_parser.add_argument("board", type=Path)
    gnd_via_parser.add_argument("output", type=Path)
    gnd_via_parser.add_argument("--x", type=float, required=True)
    gnd_via_parser.add_argument("--y", type=float, required=True)

    antenna_parser = sub.add_parser("make-antenna-corrected-candidate")
    antenna_parser.add_argument("source", type=Path)
    antenna_parser.add_argument("destination", type=Path)

    keepout_parser = sub.add_parser("formalize-antenna-keepout")
    keepout_parser.add_argument("board", type=Path)
    keepout_parser.add_argument("output", type=Path)

    args = parser.parse_args()
    if args.command == "inspect":
        inspect(args.board, set(args.nets))
    elif args.command == "make-candidate":
        make_candidate(
            args.source,
            args.destination,
            args.add_boot,
            args.boot_x,
            args.boot_y,
        )
    elif args.command == "make-manual-candidate":
        make_manual_candidate(
            args.source,
            args.destination,
            args.add_boot,
            args.boot_x,
            args.boot_y,
        )
    elif args.command == "export-dsn":
        export_dsn(args.board, args.dsn)
    elif args.command == "import-ses":
        import_ses(args.board, args.ses, args.output)
    elif args.command == "set-width":
        set_track_width(
            args.board,
            args.output,
            set(args.nets),
            args.width,
            set(args.narrow_uuid),
            args.narrow_width,
        )
    elif args.command == "transplant-routes":
        transplant_routes(
            args.target,
            args.route_source,
            args.output,
            set(args.nets),
        )
    elif args.command == "add-bottom-labels":
        add_bottom_labels(args.board, args.output)
    elif args.command == "add-ground-via":
        add_ground_via(
            args.board,
            args.output,
            args.x,
            args.y,
        )
    elif args.command == "make-antenna-corrected-candidate":
        make_antenna_corrected_candidate(args.source, args.destination)
    elif args.command == "formalize-antenna-keepout":
        formalize_antenna_keepout(args.board, args.output)


if __name__ == "__main__":
    main()
