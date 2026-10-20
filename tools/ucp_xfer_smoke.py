# SPDX-License-Identifier: MIT
"""Smoke: GET_DIRECTORY via xfer pull (requires flashed device on --port).

Usage:
  python tools/ucp_xfer_smoke.py --port COMx
"""

from __future__ import annotations

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial required") from exc

from ucp_session import APP_TLM, PKT_REQ, PKT_RSP, UcpSession, cap_tlm_only, frame
from ucp_xfer import UcpXferClient, parse_offered_rsp_body

OP_GET_DIRECTORY = 0x01


def main() -> int:
    ap = argparse.ArgumentParser(description="UCP xfer / GET_DIRECTORY smoke")
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.02)
    session = UcpSession(ser, cap_tlm_only())
    xfer = UcpXferClient(session)
    rsp_body: list[bytes | None] = [None]

    def on_tlm(pkt: int, payload: bytes) -> None:
        if pkt == PKT_RSP and len(payload) >= 4:
            c, op, st = struct.unpack_from("<HBB", payload, 0)
            if c == 1 and op == OP_GET_DIRECTORY and st == 0:
                rsp_body[0] = payload[4:]

    session.register_app(APP_TLM, on_tlm)

    try:
        session.wait_ready(timeout=5.0)
    except TimeoutError:
        print("link not READY", file=sys.stderr)
        return 1

    session.send(APP_TLM, PKT_REQ, struct.pack("<HBB", 1, OP_GET_DIRECTORY, 0))

    deadline = time.monotonic() + 2.0
    while rsp_body[0] is None and time.monotonic() < deadline:
        session.poll()
        time.sleep(0.001)

    if rsp_body[0] is None:
        print("no GET_DIRECTORY RSP", file=sys.stderr)
        return 1

    token, total = parse_offered_rsp_body(rsp_body[0])
    print(f"offered token={token} total_len={total}")
    blob = xfer.pull(token)
    print(f"pulled {len(blob)} bytes gen={blob[0]} rec_count={blob[3]}")
    print("ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
