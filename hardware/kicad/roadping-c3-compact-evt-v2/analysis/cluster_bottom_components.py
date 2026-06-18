#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import pcbnew


MOVES = {
    # AP2112 regulator cluster inside the ESP32 module footprint, between
    # the two through-hole header rows and outside the antenna keepout.
    "U2": (31.2, 44.2, 270.0),
    "C1": (34.0, 44.2, 90.0),
    "C2": (27.5, 46.5, 0.0),
    "C3": (33.8, 47.2, 90.0),
    "C4": (30.6, 48.4, 0.0),
    # OLED local bypass capacitor beside the OLED power pins.
    "C5": (42.4, 35.3, 90.0),
    # Battery-divider and ADC filter cluster beside U3 GPIO0/BAT_ADC.
    "R6": (17.7, 47.4, 90.0),
    "R7": (20.1, 47.4, 90.0),
    "C9": (22.5, 47.4, 90.0),
    # Encoder pull-ups under the encoder body, clear of mounting tabs.
    "R3": (58.3, 36.2, 270.0),
    "R4": (63.3, 36.2, 270.0),
    "R5": (63.3, 27.8, 90.0),
}

AFFECTED_NETS = {
    "/3V3",
    "/BAT_ADC",
    "/ENC_A",
    "/ENC_B",
    "/ENC_SW",
    "/SYS_RAW",
    "/VBAT",
}


def mm(value):
    return pcbnew.FromMM(value)


def point(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def net_name(item):
    net = item.GetNet()
    return net.GetNetname() if net else ""


def footprint_map(board):
    return {footprint.GetReference(): footprint for footprint in board.GetFootprints()}


def footprint_state(board, refs):
    footprints = footprint_map(board)
    state = {}
    for ref in refs:
        footprint = footprints[ref]
        state[ref] = {
            "layer": board.GetLayerName(footprint.GetLayer()),
            "position_mm": [
                round(pcbnew.ToMM(footprint.GetPosition().x), 3),
                round(pcbnew.ToMM(footprint.GetPosition().y), 3),
            ],
            "orientation_deg": round(footprint.GetOrientationDegrees(), 3),
            "pads": [
                {
                    "number": pad.GetNumber(),
                    "net": pad.GetNetname(),
                    "position_mm": [
                        round(pcbnew.ToMM(pad.GetPosition().x), 3),
                        round(pcbnew.ToMM(pad.GetPosition().y), 3),
                    ],
                }
                for pad in footprint.Pads()
            ],
        }
    return state


def remove_affected_routes(board):
    removed = []
    for item in list(board.GetTracks()):
        name = net_name(item)
        if name not in AFFECTED_NETS:
            continue
        removed.append(
            {
                "kind": "via" if isinstance(item, pcbnew.PCB_VIA) else "track",
                "net": name,
            }
        )
        board.Remove(item)
    return removed


def move_components(source, destination):
    board = pcbnew.LoadBoard(str(source))
    footprints = footprint_map(board)
    before = footprint_state(board, MOVES)

    for ref, (x_mm, y_mm, angle_deg) in MOVES.items():
        footprint = footprints[ref]
        if footprint.GetLayer() != pcbnew.B_Cu:
            raise RuntimeError(f"{ref} is not on B.Cu")
        footprint.SetPosition(point(x_mm, y_mm))
        footprint.SetOrientationDegrees(angle_deg)

    after = footprint_state(board, MOVES)
    removed = remove_affected_routes(board)
    pcbnew.SaveBoard(str(destination), board)

    print(
        json.dumps(
            {
                "source": str(source),
                "destination": str(destination),
                "before": before,
                "after": after,
                "affected_nets": sorted(AFFECTED_NETS),
                "removed_routes": len(removed),
                "removed_by_net": {
                    name: sum(1 for item in removed if item["net"] == name)
                    for name in sorted(AFFECTED_NETS)
                },
            },
            indent=2,
        )
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    move_components(args.source, args.destination)


if __name__ == "__main__":
    main()
