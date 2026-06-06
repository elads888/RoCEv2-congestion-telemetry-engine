#!/usr/bin/env python3
"""
=============================================================================
check_doorbell_order.py — Doorbell write-order validator
This hook runs after every write to rocev2_header.h

The telemetry engine's correctness rests on one invariant:
the simulator writes sequence_number last into the shared memory page.
This is the doorbell pattern - sequence_number tells the TelemetryThread
that all other fields (congestion_flag, etc.) have been committed and
are safe to read.

This script checks the actual write order in simulator.cpp.

If sequence_number is not written last, a concurrent reader may see a
new sequence_number before congestion_flag has been updated — a possible race condition.
"""

import sys
import re
import os

def check_write_order(simulator_path: str) -> bool:
    try:
        with open(simulator_path, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"[doorbell-check] ERROR: file not found: {simulator_path}", file=sys.stderr)
        return False

    # Find all hdr->field = assignments in source order.
    write_pattern = re.compile(r'hdr->([a-zA-Z_][a-zA-Z0-9_]*)\s*=')
    writes = write_pattern.findall(content)

    if not writes:
        print(
            f"[doorbell-check] WARNING: no hdr-> writes found in {simulator_path}. "
            "Is the write loop still present? Regex may need updating.",
            file=sys.stderr
        )
        # Fail open — don't block on a regex miss, but warn loudly.
        return True

    last_write = writes[-1]

    if last_write != 'sequence_number':
        print(
            f"[doorbell-check] VIOLATION: last hdr-> write in simulator is "
            f"'{last_write}', expected 'sequence_number'.\n"
            f"  Writes found (in source order): {writes}\n"
            f"  The doorbell invariant requires sequence_number to be written "
            f"last — it signals TelemetryThread that all payload fields are "
            f"already committed to shared memory.",
            file=sys.stderr
        )
        return False

    print(
        f"[doorbell-check] OK - sequence_number is written last among "
        f"{len(writes)} hdr-> writes: {writes}"
    )
    return True


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path/to/rocev2_header.h>", file=sys.stderr)
        sys.exit(1)

    header_path = sys.argv[1]
    src_dir = os.path.dirname(os.path.abspath(header_path))
    simulator_path = os.path.join(src_dir, 'simulator.cpp')

    ok = check_write_order(simulator_path)
    sys.exit(0 if ok else 1)
