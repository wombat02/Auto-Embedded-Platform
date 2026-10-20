#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PT profile A generator — CAN0 frames 0x201/0x202 (same as pt_can_gen.py)."""

from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aep_demo_pt import (  # noqa: E402
    PT_FRAME_A_POWERTRAIN,
    PT_FRAME_A_TORQUE,
    encode_pt_powertrain,
    encode_pt_torque,
)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="PT profile A generator (0x201/0x202)")
    ap.add_argument("--interface", default="kvaser")
    ap.add_argument("--channel", default="0")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--period-ms", type=int, default=20)
    ap.add_argument("--sweep-s", type=float, default=2.0, help="eng value sine period (s)")
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
        f"PT-A gen on {args.interface}:{channel} "
        f"ids=0x{PT_FRAME_A_POWERTRAIN:03X}/0x{PT_FRAME_A_TORQUE:03X}",
        flush=True,
    )
    t0 = time.monotonic()
    period = max(0.005, args.period_ms / 1000.0)
    w = (2.0 * math.pi) / max(0.2, args.sweep_s)
    try:
        while True:
            t = time.monotonic() - t0
            rpm = 800.0 + 3200.0 * (0.5 + 0.5 * math.sin(t * w))
            cool = 70.0 + 25.0 * (0.5 + 0.5 * math.sin(t * w + 1.2))
            torque = 50.0 + 200.0 * (0.5 + 0.5 * math.sin(t * w + 2.1))
            bus.send(
                can.Message(
                    arbitration_id=PT_FRAME_A_POWERTRAIN,
                    data=encode_pt_powertrain(rpm, cool),
                    is_extended_id=False,
                )
            )
            bus.send(
                can.Message(
                    arbitration_id=PT_FRAME_A_TORQUE,
                    data=encode_pt_torque(torque),
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
