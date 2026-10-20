#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Tier B dash generator — CAN1 frames 0x401/0x402 (steering + stalks)."""

from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aep_demo_pt import DASH_FRAME_STALKS, DASH_FRAME_STEER, encode_dash_stalks, encode_dash_steer


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Dash generator (0x401/0x402)")
    ap.add_argument("--interface", default="kvaser")
    ap.add_argument("--channel", default="1")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--period-ms", type=int, default=20)
    ap.add_argument("--sweep-s", type=float, default=2.0, help="steer/stalk cycle period (s)")
    args = ap.parse_args(argv)

    try:
        import can
    except ImportError as exc:
        raise SystemExit("pip install python-can") from exc

    channel: int | str = args.channel
    try:
        channel = int(args.channel)
    except ValueError:
        pass

    bus = can.Bus(interface=args.interface, channel=channel, bitrate=args.bitrate)
    print(
        f"dash gen on {args.interface}:{channel} "
        f"ids=0x{DASH_FRAME_STEER:03X}/0x{DASH_FRAME_STALKS:03X}",
        flush=True,
    )
    t0 = time.monotonic()
    period = max(0.005, args.period_ms / 1000.0)
    w = (2.0 * math.pi) / max(0.2, args.sweep_s)
    try:
        while True:
            t = time.monotonic() - t0
            steer = 45.0 * math.sin(t * w)
            turn_l = math.sin(t * w) > 0.3
            turn_r = math.sin(t * w) < -0.3
            wiper = math.sin(t * w + 1.0) > 0.0
            high = math.sin(t * w + 2.0) > 0.5
            bus.send(
                can.Message(
                    arbitration_id=DASH_FRAME_STEER,
                    data=encode_dash_steer(steer),
                    is_extended_id=False,
                )
            )
            bus.send(
                can.Message(
                    arbitration_id=DASH_FRAME_STALKS,
                    data=encode_dash_stalks(
                        turn_l=turn_l, turn_r=turn_r, wiper=wiper, high_beam=high
                    ),
                    is_extended_id=False,
                )
            )
            time.sleep(period)
    except KeyboardInterrupt:
        print("stopped", flush=True)
    finally:
        bus.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
