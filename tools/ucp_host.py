#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
UCP host session over a USB CDC (or other) serial port.

Speaks control HELLO/CAPABILITY/LINK_READY, then shows a fixed-frame
bar-graph dashboard for APP 0x20 / version 0x0100 (V1.0) live TLV frames.

Requires: pyserial
  pip install pyserial

Example:
  python tools/ucp_host.py --port COM7
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from term_dashboard import Dashboard  # noqa: E402
from ucp_session import UcpSession, cap_tlm_only  # noqa: E402
from ucp_tlm_feed import TlmFeed, add_demo_rows  # noqa: E402

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial is required: pip install pyserial") from exc


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="UCP host dashboard for demo telemetry (APP 0x20)")
    ap.add_argument("--port", required=True, help="Serial CDC port, e.g. COM7 or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args(argv)

    dash = Dashboard(title=f"ucp_host  —  {args.port}  APP 0x20", stale_s=2.5, bar_width=28)
    add_demo_rows(dash)
    dash.set_status("opening…")

    ser = serial.Serial(args.port, args.baud, timeout=0)
    try:
        ser.dtr = True
        ser.rts = True
    except Exception:
        pass

    session = UcpSession(ser, cap_tlm_only())
    feed = TlmFeed(session, dash)
    session.set_on_ready(feed.refresh_status)
    session.set_on_not_ready(feed.on_session_not_ready)
    feed.refresh_status()

    last_draw = 0.0
    try:
        while True:
            session.tick()
            session.drain_available()
            chunk = session.drain_once(arm_s=0.002)
            if chunk:
                session.pump(chunk)
                session.drain_available()
            now = time.monotonic()
            if (now - last_draw) >= 0.05:
                last_draw = now
                age = now - dash.link_rx_stamp if dash.link_rx_stamp else 99.0
                if age > 2.5:
                    feed.rx_ok = False
                    feed.refresh_status()
                dash.draw()
    except KeyboardInterrupt:
        return 0
    finally:
        dash.close()


if __name__ == "__main__":
    sys.exit(main())
