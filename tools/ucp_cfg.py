#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""UCP config CLI (APP 0x71) — list / slots / apply / save / load / ads."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial is required: pip install pyserial") from exc

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aep_demo_pt import PACK_DEMO_DASH  # noqa: E402
from ucp_cfg_client import CfgServiceClient  # noqa: E402
from ucp_session import UcpSession, cap_cfg_only  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description="UCP config tool (APP 0x71)")
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list")
    sub.add_parser("slots")
    sub.add_parser("apply")
    sub.add_parser("save")
    sub.add_parser("load")
    sub.add_parser("ads")

    p_set = sub.add_parser("set")
    p_set.add_argument("comp", choices=["pt", "pid", "cluster", "dash"])
    p_set.add_argument("--enable", type=int, choices=[0, 1], default=1)
    p_set.add_argument("--profile", choices=["a", "b"], default="a", help="PT profile")
    p_set.add_argument("--bus", type=int, default=None)

    args = ap.parse_args()
    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    try:
        ser.dtr = True
        ser.rts = True
    except Exception:
        pass

    session = UcpSession(ser, cap_cfg_only())
    cfg = CfgServiceClient(session, auto_pump=True)
    print(f"opened {args.port}; negotiating UCP for app 0x71", flush=True)
    session.wait_ready()
    print("UCP link READY", flush=True)

    if args.cmd == "list":
        descs = cfg.list_descs()
        print(f"descriptors ({len(descs)}):")
        for d in descs:
            print(f"  id=0x{d.comp_id:04X} kind={d.kind} max_cfg={d.max_cfg} name={d.name}")
    elif args.cmd == "slots":
        slots = cfg.get_slots()
        print(f"slots ({len(slots)}):")
        for s in slots:
            print(f"  id=0x{s.comp_id:04X} enabled={int(s.enabled)} cfg={s.cfg.hex()}")
    elif args.cmd == "apply":
        cfg.apply()
        print("applied")
    elif args.cmd == "save":
        cfg.save()
        print("saved")
    elif args.cmd == "load":
        cfg.load()
        print("loaded")
    elif args.cmd == "ads":
        ads = cfg.get_ads()
        print(f"pack ads ({len(ads)}):")
        for pid, rev in ads:
            tag = "demo_dash" if pid == PACK_DEMO_DASH else f"0x{pid:04X}"
            print(f"  pack={tag} rev=0x{rev:04X}")
    elif args.cmd == "set":
        en = bool(args.enable)
        if args.comp == "pt":
            bus = 0 if args.bus is None else args.bus
            cfg.set_pt(en, args.profile, bus)
            print(f"set pt enabled={int(en)} profile={args.profile} bus={bus}")
        else:
            cfg.set_bus_comp(args.comp, en, args.bus)
            print(f"set {args.comp} enabled={int(en)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
