#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Sniff mock instrument-cluster frames on Kvaser channel (default 1 / board CAN1).

Fixed-frame bar dashboard for 0x301: RPM, coolant C, torque Nm.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aep_demo_pt import CLUSTER_FRAME_ID, decode_cluster  # noqa: E402
from term_dashboard import Dashboard  # noqa: E402


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Sniff cluster 0x301 on CAN1 (dashboard)")
    ap.add_argument("--interface", default="kvaser")
    ap.add_argument("--channel", default="1")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--seconds", type=float, default=0.0, help="0 = run until Ctrl+C")
    ap.add_argument("--expect", type=int, default=1, help="min frames required (exit 2 if fewer)")
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

    dash = Dashboard(
        title=f"cluster_sniff  —  {args.interface}:{channel}  id=0x{CLUSTER_FRAME_ID:03X}",
        stale_s=0.5,
        bar_width=28,
    )
    dash.add("rpm", "RPM", "rpm", 0, 7000)
    dash.add("coolant", "Coolant", "C", -40, 120)
    dash.add("torque", "Torque", "Nm", -50, 400)
    dash.set_status("waiting for frames…")

    bus = can.Bus(interface=args.interface, channel=channel, bitrate=args.bitrate)
    seen = 0
    last_draw = 0.0
    deadline = time.monotonic() + args.seconds if args.seconds > 0 else None
    try:
        while True:
            now = time.monotonic()
            if deadline is not None and now >= deadline:
                break
            msg = bus.recv(timeout=0.05)
            if msg is not None and int(msg.arbitration_id) == CLUSTER_FRAME_ID:
                rpm, cool, tq = decode_cluster(bytes(msg.data))
                seen += 1
                dash.touch_link(now)
                dash.set("rpm", float(rpm))
                dash.set("coolant", float(cool))
                dash.set("torque", float(tq))
                dash.set_status(f"RX OK  |  frames={seen}")
            if (now - last_draw) >= 0.05:
                last_draw = now
                if seen == 0:
                    dash.set_status("waiting for frames…")
                dash.draw()
    except KeyboardInterrupt:
        pass
    finally:
        dash.close()
        bus.shutdown()

    # Final one-line summary after cursor restore (scrolls once — intentional).
    print(f"saw {seen} cluster frame(s)", flush=True)
    if seen < args.expect:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
