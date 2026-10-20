# SPDX-License-Identifier: MIT
"""Host manifest for Tier B demo dash pack (pack_id=0xD001 rev=0x0001)."""

from __future__ import annotations

from typing import Dict, Tuple

PACK_ID = 0xD001
REVISION = 0x0001
NAME = "demo_dash"

# id -> (type, mult, div, offset, name)  eng = raw*mult/div + offset
Entry = Tuple[int, int, int, int, str]

SIGNALS: Dict[int, Entry] = {
    0xF010: (0x05, 1, 1, 0, "steer_deg"),  # i16 degrees
    0xF011: (0x02, 1, 1, 0, "turn_left"),  # u8
    0xF012: (0x02, 1, 1, 0, "turn_right"),
    0xF013: (0x02, 1, 1, 0, "wiper"),
    0xF014: (0x02, 1, 1, 0, "high_beam"),
}


def scale_display(sid: int, typ: int, raw: int) -> str:
    e = SIGNALS.get(sid)
    if e is None:
        return f"id=0x{sid:04X} raw={raw}"
    _t, mult, div, offset, name = e
    if typ in (0x03, 0x05, 0x07):  # signed
        width = {0x03: 1, 0x05: 2, 0x07: 4}[typ]
        signed = int.from_bytes(raw.to_bytes(width, "little", signed=False), "little", signed=True)
        eng = (signed * mult) / div + offset
    else:
        eng = (raw * mult) / div + offset
    return f"{name}={eng:g}"
