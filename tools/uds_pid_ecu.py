#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Dummy UDS ECU on CAN (default Kvaser ch0 / board CAN0).

Listens on 0x7E0, responds on 0x7E8: DiagnosticSessionControl, TesterPresent,
RDBI for DIDs 0xF201 (boost) and 0xF202 (fuel pressure).
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aep_demo_pt import DID_BOOST, DID_FUEL_PRESS, UDS_REQ_ID, UDS_RESP_ID


def _be_u16(v: int) -> bytes:
    return struct.pack(">H", v & 0xFFFF)


def handle_request(payload: bytes, boost: int, fuel: int) -> bytes | None:
    if not payload:
        return None
    sid = payload[0]
    if sid == 0x10 and len(payload) >= 2:
        # Default session positive: 50 01 + P2 timings
        return bytes([0x50, payload[1], 0x00, 0x32, 0x01, 0xF4])
    if sid == 0x3E:
        return bytes([0x7E, payload[1] if len(payload) > 1 else 0x00])
    if sid == 0x22 and len(payload) >= 3:
        did = (payload[1] << 8) | payload[2]
        if did == DID_BOOST:
            return bytes([0x62, payload[1], payload[2]]) + _be_u16(boost)
        if did == DID_FUEL_PRESS:
            return bytes([0x62, payload[1], payload[2]]) + _be_u16(fuel)
        return bytes([0x7F, 0x22, 0x31])  # requestOutOfRange
    return bytes([0x7F, sid, 0x11])  # serviceNotSupported


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Dummy UDS PID ECU (0x7E0/0x7E8)")
    ap.add_argument("--interface", default="kvaser")
    ap.add_argument("--channel", default="0")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--sweep-s", type=float, default=2.0, help="DID value sine period (s)")
    args = ap.parse_args(argv)

    try:
        import can
        import isotp
    except ImportError as exc:
        raise SystemExit("pip install python-can can-isotp") from exc

    channel: int | str = args.channel
    try:
        channel = int(args.channel)
    except ValueError:
        pass

    bus = can.Bus(interface=args.interface, channel=channel, bitrate=args.bitrate)
    # Server: RX requests on 0x7E0, TX responses on 0x7E8
    addr = isotp.Address(
        isotp.AddressingMode.Normal_11bits, txid=UDS_RESP_ID, rxid=UDS_REQ_ID
    )
    stack = isotp.CanStack(bus=bus, address=addr, params={"tx_padding": 0x00})
    stack.start()
    print(
        f"UDS PID ECU on {args.interface}:{channel} "
        f"rx=0x{UDS_REQ_ID:03X} tx=0x{UDS_RESP_ID:03X} "
        f"DID 0x{DID_BOOST:04X}/0x{DID_FUEL_PRESS:04X}",
        flush=True,
    )
    t0 = time.monotonic()
    w = (2.0 * math.pi) / max(0.2, args.sweep_s)
    try:
        while True:
            t = time.monotonic() - t0
            boost = int(150 + 50 * (0.5 + 0.5 * math.sin(t * w)))  # raw/100 bar
            fuel = int(300 + 100 * (0.5 + 0.5 * math.sin(t * w + 1.0)))  # kPa
            msg = stack.recv(block=True, timeout=0.05)
            if msg:
                req = bytes(msg)
                rsp = handle_request(req, boost, fuel)
                print(f"req {req.hex()} -> {rsp.hex() if rsp else 'drop'}", flush=True)
                if rsp:
                    stack.send(rsp)
    except KeyboardInterrupt:
        print("stopped", flush=True)
    finally:
        try:
            stack.stop()
        except Exception:
            pass
        bus.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
