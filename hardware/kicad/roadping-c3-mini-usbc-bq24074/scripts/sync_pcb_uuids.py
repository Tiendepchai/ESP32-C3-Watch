#!/usr/bin/env python3
import sys, os, re

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_sch")
PCB = os.path.join(PROJ, "roadping-c3-mini-usbc-bq24074.kicad_pcb")

# Find all symbol references and uuids in schematic
with open(SCH) as f:
    sch_text = f.read()

# Get sheet UUID
sheet_uuid_match = re.search(r'\(uuid\s+\"([a-f0-9\-]+)\"\)', sch_text)
sheet_uuid = sheet_uuid_match.group(1) if sheet_uuid_match else ""
print(f"Schematic sheet UUID: {sheet_uuid}")

# Find symbol references and their uuids
# We search for symbol blocks
sch_map = {}
# Find all (symbol (lib_id ...) ...)
symbol_blocks = re.findall(r'\(symbol\s+.*?(?=\(symbol|\Z)', sch_text, re.DOTALL)
for block in symbol_blocks:
    ref_match = re.search(r'\(property\s+\"Reference\"\s+\"([^\"]+)\"', block)
    uuid_match = re.search(r'\(uuid\s+\"([a-f0-9\-]+)\"\)', block)
    if ref_match and uuid_match:
        ref = ref_match.group(1)
        uid = uuid_match.group(1)
        sch_map[ref] = f"/{sheet_uuid}/{uid}"

print(f"Schematic mapped {len(sch_map)} symbol UUID paths:")
for r, p in sorted(sch_map.items())[:10]:
    print(f"  {r:6s} -> {p}")

# Load PCB file
with open(PCB) as f:
    pcb_text = f.read()

# Find all footprint blocks
# KiCad 10 footprint blocks start with (footprint "...") and end with a matching paren.
# Since S-expressions are nested, we can use a paren-matching scan to split footprint blocks.
pos = 0
footprints = []
while True:
    pos = pcb_text.find('(footprint ', pos)
    if pos == -1:
        break
    
    # Paren matching
    start = pos
    depth = 0
    end = -1
    for i in range(start, len(pcb_text)):
        if pcb_text[i] == '(':
            depth += 1
        elif pcb_text[i] == ')':
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    
    if end != -1:
        block = pcb_text[start:end]
        footprints.append((start, end, block))
        pos = end
    else:
        pos += 1

print(f"\nPCB contains {len(footprints)} footprint blocks.")

# We want to separate:
# - on-board footprints (placed near coordinates 0-80mm)
# - off-board duplicate footprints (placed near coordinates 140-170mm)
on_board = []
off_board = []

for start, end, block in footprints:
    # Get reference
    ref_match = re.search(r'\(property\s+\"Reference\"\s+\"([^\"]+)\"', block)
    if not ref_match:
        continue
    ref = ref_match.group(1)
    
    # Get coordinates
    at_match = re.search(r'\(at\s+([0-9\.\-]+)\s+([0-9\.\-]+)', block)
    if not at_match:
        continue
    x = float(at_match.group(1))
    y = float(at_match.group(2))
    
    if x > 100 or y > 100:
        off_board.append((start, end, ref, x, y, block))
    else:
        on_board.append((start, end, ref, x, y, block))

print(f"On-board footprints: {len(on_board)}")
print(f"Off-board duplicate footprints: {len(off_board)}")

# For each on-board footprint, let's update its path property to match the schematic UUID path!
# In KiCad 10, the path is in: (path "/sheet_uuid/symbol_uuid") or (property "path" ...)
updated_pcb_text = pcb_text
offset = 0

# Sort on_board by start coordinate so we can do in-place text replacement safely
on_board.sort(key=lambda x: x[0])

modified_count = 0
for start, end, ref, x, y, block in on_board:
    if ref not in sch_map:
        print(f"  Warning: On-board reference {ref} not found in schematic map.")
        continue
    
    target_path = sch_map[ref]
    
    # Check if (path "...") exists in the footprint block
    path_match = re.search(r'\(path\s+\"([^\"]+)\"\)', block)
    if path_match:
        # Replace existing path
        old_path = path_match.group(0)
        new_path = f'(path "{target_path}")'
        new_block = block.replace(old_path, new_path)
    else:
        # Insert (path "...") just before the closing paren of the footprint block
        # Footprints usually end with ')'
        new_path = f'\n\t\t(path "{target_path}")\n\t'
        last_paren = block.rfind(')')
        new_block = block[:last_paren] + new_path + block[last_paren:]
        
    # Replace the block in pcb_text
    start_offset = start + offset
    end_offset = end + offset
    updated_pcb_text = updated_pcb_text[:start_offset] + new_block + updated_pcb_text[end_offset:]
    offset += len(new_block) - len(block)
    modified_count += 1

print(f"Updated paths for {modified_count} on-board footprints.")

# Now, we need to DELETE the off-board duplicates from the board file!
# We re-parse the updated pcb text to get fresh coordinates (since offsets shifted)
pos = 0
footprints_rev = []
while True:
    pos = updated_pcb_text.find('(footprint ', pos)
    if pos == -1:
        break
    
    # Paren matching
    start = pos
    depth = 0
    end = -1
    for i in range(start, len(updated_pcb_text)):
        if updated_pcb_text[i] == '(':
            depth += 1
        elif updated_pcb_text[i] == ')':
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    
    if end != -1:
        block = updated_pcb_text[start:end]
        footprints_rev.append((start, end, block))
        pos = end
    else:
        pos += 1

# Delete off-board blocks from bottom to top so offsets don't shift for preceding blocks
delete_count = 0
footprints_rev.sort(key=lambda x: x[0], reverse=True)
for start, end, block in footprints_rev:
    # Check coordinate
    at_match = re.search(r'\(at\s+([0-9\.\-]+)\s+([0-9\.\-]+)', block)
    if at_match:
        x = float(at_match.group(1))
        y = float(at_match.group(2))
        if x > 100 or y > 100:
            # Delete this block
            # also strip preceding newline or trailing newline
            updated_pcb_text = updated_pcb_text[:start] + updated_pcb_text[end:]
            delete_count += 1

print(f"Deleted {delete_count} off-board duplicate footprints.")

# Write back to PCB file
with open(PCB, 'w') as f:
    f.write(updated_pcb_text)

print("PCB synchronization complete.")
