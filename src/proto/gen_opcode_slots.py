#!/usr/bin/env python3
"""Generate src/proto/OpcodeSlots.inc from the reversed opcode_slots.json."""
import json, sys, io

SRC = "opcode_slots.json"   # the reversed table, kept out of tree
OUT = "OpcodeSlots.inc"

# The eight connection-control opcodes (RESEARCH D2): (op & 0xB3FD) == 0x140.
CONTROL_MASK, CONTROL_VALUE = 0xB3FD, 0x0140

data = json.load(open(SRC))

rows = []
for k, v in data.items():
    op = int(k, 16)
    rows.append((op, v["name"], int(v["send_slot"]), int(v["recv_slot"]), v["rule"]))
rows.sort()

gated = [r for r in rows if r[3] == 1]
control = [r for r in rows if (r[0] & CONTROL_MASK) == CONTROL_VALUE]

# Cross-check the two facts the architecture treats as law.
assert len(gated) == 19, "expected 19 recv-gated opcodes, got %d" % len(gated)
assert all(r[2] == 1 for r in gated), "a recv-gated opcode has send_slot 0"
assert len(control) == 8, "expected 8 connection-control opcodes, got %d" % len(control)
assert all(r[2] == 0 and r[3] == 0 for r in control), "a control opcode is not slot 0/0"

buf = io.StringIO()
buf.write("""// SPDX-License-Identifier: GPL-3.0-or-later
//
// GENERATED -- do not edit. Regenerate with src/proto/gen_opcode_slots.py.
//
// Which of the two world TCP streams each 4.3.4 build 15595 opcode belongs to,
// read out of the client's own send router (send_slot) and receive gate
// (recv_slot). %d opcodes; %d are recv-gated and %d are connection-control.
//
// SLOT(opcode, send_slot, recv_slot)

""" % (len(rows), len(gated), len(control)))

for op, name, s, r, rule in rows:
    buf.write("SLOT(0x%04X, %d, %d) // %s\n" % (op, s, r, name))

open(OUT, "w", newline="\n").write(buf.getvalue())

print("wrote %s: %d opcodes, %d gated, %d control" %
      (OUT, len(rows), len(gated), len(control)))
print("gated:", ", ".join("0x%04X %s" % (r[0], r[1]) for r in gated))
print("control:", ", ".join("0x%04X %s" % (r[0], r[1]) for r in control))
