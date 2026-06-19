#!/usr/bin/env python3
import json
import math
import sys

import pcbnew


def to_mm(value):
    return pcbnew.ToMM(value)


def point(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def segment_distance(px, py, ax, ay, bx, by):
    dx = bx - ax
    dy = by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def main():
    board = pcbnew.LoadBoard(sys.argv[1])
    back_zone = next(
        zone for zone in board.Zones() if zone.GetZoneName() == "EVT_V2_GND_ZONE_BACK"
    )
    front_zone = next(
        zone for zone in board.Zones() if zone.GetZoneName() == "EVT_V2_GND_ZONE_FRONT"
    )
    back_fill = back_zone.GetFilledPolysList(pcbnew.B_Cu)
    front_fill = front_zone.GetFilledPolysList(pcbnew.F_Cu)
    back_island = back_fill.Outline(3)
    front_main = front_fill.Outline(0)
    bbox = back_island.BBox()

    obstacles = []
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            pos = pad.GetPosition()
            box = pad.GetBoundingBox()
            obstacles.append(
                (
                    "pad",
                    pad.GetNetname(),
                    to_mm(pos.x),
                    to_mm(pos.y),
                    0.5 * max(to_mm(box.GetWidth()), to_mm(box.GetHeight())),
                )
            )

    for item in board.GetTracks():
        if isinstance(item, pcbnew.PCB_VIA):
            pos = item.GetPosition()
            obstacles.append(
                (
                    "via",
                    item.GetNetname(),
                    to_mm(pos.x),
                    to_mm(pos.y),
                    0.5 * to_mm(item.GetWidth(pcbnew.F_Cu)),
                )
            )
        else:
            start = item.GetStart()
            end = item.GetEnd()
            obstacles.append(
                (
                    "track",
                    item.GetNetname(),
                    to_mm(start.x),
                    to_mm(start.y),
                    to_mm(end.x),
                    to_mm(end.y),
                    0.5 * to_mm(item.GetWidth()),
                )
            )

    candidates = []
    x = to_mm(bbox.GetLeft())
    while x <= to_mm(bbox.GetRight()):
        y = to_mm(bbox.GetTop())
        while y <= to_mm(bbox.GetBottom()):
            candidate = point(x, y)
            if back_island.PointInside(candidate) and front_main.PointInside(candidate):
                minimum = 999.0
                nearest = None
                for obstacle in obstacles:
                    if obstacle[0] == "track":
                        _, net, ax, ay, bx, by, radius = obstacle
                        distance = segment_distance(x, y, ax, ay, bx, by) - radius
                    else:
                        kind, net, ox, oy, radius = obstacle
                        distance = math.hypot(x - ox, y - oy) - radius
                    if distance < minimum:
                        minimum = distance
                        nearest = [obstacle[0], obstacle[1]]
                candidates.append(
                    {
                        "x": round(x, 3),
                        "y": round(y, 3),
                        "minimum_copper_clearance_mm": round(minimum, 3),
                        "nearest": nearest,
                    }
                )
            y += 0.25
        x += 0.25

    candidates.sort(key=lambda item: item["minimum_copper_clearance_mm"], reverse=True)
    print(json.dumps(candidates[:30], indent=2))


if __name__ == "__main__":
    main()
