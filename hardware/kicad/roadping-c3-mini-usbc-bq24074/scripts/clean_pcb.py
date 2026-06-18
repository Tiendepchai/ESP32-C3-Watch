#!/usr/bin/env python3
"""Remove duplicate/stale footprints from BQ24074 PCB.

Board is 80x50mm. PCB has stale footprints from FreeRouting passes.
Keep only one copy per reference within board bounds.
"""
import os, sys, re, shutil

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")
DRY_RUN = "--dry-run" in sys.argv

BX_MIN, BX_MAX = 0.0, 80.0
BY_MIN, BY_MAX = 0.0, 50.0

def is_in_board(x, y):
    return BX_MIN <= x <= BX_MAX and BY_MIN <= y <= BY_MAX

def find_footprint_blocks(lines):
    """Find all (footprint ... ) blocks with their ref/position/type."""
    blocks = []
    i = 0
    while i < len(lines):
        ls = lines[i].strip()
        if ls.startswith('(footprint') or ls.startswith('(module'):
            start = i
            depth = 0
            while i < len(lines):
                depth += lines[i].count('(') - lines[i].count(')')
                if depth <= 0:
                    break
                i += 1
            end = i  # inclusive

            block = '\n'.join(lines[start:end+1])

            # Extract ref
            ref_m = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            ref = ref_m.group(1) if ref_m else '?'

            # Extract position
            at_m = re.search(r'\(at\s+([\d.-]+)\s+([\d.-]+)', block)
            x = float(at_m.group(1)) if at_m else 0
            y = float(at_m.group(2)) if at_m else 0

            # Extract type
            tp_m = re.search(r'\(attr\s+(\w+)', block)
            ftype = tp_m.group(1) if tp_m else 'unspecified'

            blocks.append((start, end, ref, x, y, ftype))

        i += 1
    return blocks

def run():
    with open(PCB) as f:
        lines = f.readlines()

    blocks = find_footprint_blocks(lines)
    print(f"Found {len(blocks)} footprint blocks")

    # Group by ref
    by_ref = {}
    for i, (s, e, ref, x, y, ftype) in enumerate(blocks):
        by_ref.setdefault(ref, []).append((i, s, e, x, y, ftype))

    # Check for out-of-board duplicates
    to_remove_indices = set()
    total_remove = 0

    for ref, copies in sorted(by_ref.items()):
        if len(copies) == 1:
            continue

        # Separate in-board and out-of-board copies
        in_board = [c for c in copies if is_in_board(c[3], c[4])]
        out_board = [c for c in copies if not is_in_board(c[3], c[4])]

        # Remove all out-of-board copies
        for c in out_board:
            print(f"  REMOVE {ref} off-board @({c[3]:.1f},{c[4]:.1f}) {c[5]}")
            to_remove_indices.add(c[0])
            total_remove += 1

        # Handle in-board duplicates
        if len(in_board) > 1:
            # Group by position
            pos_groups = {}
            for c in in_board:
                px, py = round(c[3], 1), round(c[4], 1)
                pos_groups.setdefault((px, py), []).append(c)

            for pos, pos_copies in pos_groups.items():
                if len(pos_copies) > 1:
                    # Same position — check if different types (legit)
                    types = set(c[5] for c in pos_copies)
                    if len(types) > 1:
                        print(f"  KEEP {ref} x{len(pos_copies)} at {pos} (different types: {types})")
                    else:
                        # Same type duplicates — keep first, remove rest
                        for c in pos_copies[1:]:
                            print(f"  REMOVE {ref} dup @{pos} {c[5]}")
                            to_remove_indices.add(c[0])
                            total_remove += 1

    if total_remove == 0:
        print("\nNo duplicates to remove.")
        return

    # Remove blocks (reverse order)
    remove_blocks = sorted([blocks[i] for i in to_remove_indices],
                           key=lambda x: x[0], reverse=True)

    for s, e, ref, x, y, ftype in remove_blocks:
        del lines[s:e+1]

    print(f"\nRemoved {total_remove} footprint blocks")
    print(f"Remaining: {len(blocks) - total_remove} footprint blocks")

    if DRY_RUN:
        print("DRY RUN — no changes")
    else:
        backup = PCB + '.clean.bak'
        shutil.copy2(PCB, backup)
        print(f"Backup: {backup}")
        with open(PCB, 'w') as f:
            f.writelines(lines)
        print(f"Written: {PCB}")

    # Verify remaining
    remaining = find_footprint_blocks(lines if DRY_RUN else open(PCB).readlines())
    print(f"\nVerification - {len(remaining)} footprint blocks remain")

if __name__ == '__main__':
    run()
