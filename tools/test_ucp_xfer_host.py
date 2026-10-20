# SPDX-License-Identifier: MIT
"""Offline unit checks for host xfer helpers (no serial)."""

from __future__ import annotations

import struct
import sys

from ucp_xfer import (
    ACK_OK,
    FLAG_FIRST,
    FLAG_LAST,
    PKT_PUSH_ACK,
    PKT_PUSH_DATA,
    SVC_FLAG_OFFERED,
    parse_offered_rsp_body,
)


def test_parse_offered() -> None:
    body = bytes([SVC_FLAG_OFFERED]) + struct.pack("<HH", 42, 100)
    token, total = parse_offered_rsp_body(body)
    assert token == 42 and total == 100


def test_push_frame_layout() -> None:
    token, seq, flags = 7, 0, FLAG_FIRST | FLAG_LAST
    data = b"hi"
    payload = struct.pack("<HHB", token, seq, flags) + data
    assert len(payload) == 5 + 2
    t, s, f = struct.unpack_from("<HHB", payload, 0)
    assert t == 7 and s == 0 and (f & FLAG_FIRST) and (f & FLAG_LAST)
    ack = struct.pack("<HHB", token, seq, ACK_OK)
    assert len(ack) == 5
    assert PKT_PUSH_DATA == 0x10 and PKT_PUSH_ACK == 0x11


def main() -> int:
    test_parse_offered()
    test_push_frame_layout()
    print("ok: host xfer helpers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
