# SPDX-License-Identifier: MIT
"""APP 0x20 live TLV feed → term_dashboard.Dashboard."""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple

from aep_demo_pt import (
    SIG_DASH_HIGH_BEAM,
    SIG_DASH_STEER_DEG,
    SIG_DASH_TURN_L,
    SIG_DASH_TURN_R,
    SIG_DASH_WIPER,
)
from sig_catalog_host import decode_eng
from term_dashboard import Dashboard
from ucp_session import (
    APP_TLM,
    PKT_CHANGE,
    PKT_HF,
    PKT_LF,
    PKT_LIVE,
    PKT_MF,
    UcpSession,
)

TLM_LIVE_PKTS = (PKT_LIVE, PKT_HF, PKT_MF, PKT_LF, PKT_CHANGE)

SIG_ENGINE_RPM = 0x0004
SIG_COOLANT_TEMP = 0x0005
SIG_ENGINE_TORQUE = 0x0006
SIG_EXT_BOOST = 0xF001
SIG_EXT_FUEL_PRESS = 0xF002

SIG_TLV_Q_VALID = 0x01

WIDTH = {
    0x01: 1,
    0x02: 1,
    0x03: 1,
    0x04: 2,
    0x05: 2,
    0x06: 4,
    0x07: 4,
    0x08: 1,
    0x10: 4,
}

DASH_KEYS: Dict[int, str] = {
    SIG_ENGINE_RPM: "rpm",
    SIG_COOLANT_TEMP: "coolant",
    SIG_ENGINE_TORQUE: "torque",
    SIG_EXT_BOOST: "boost",
    SIG_EXT_FUEL_PRESS: "fuel",
    SIG_DASH_STEER_DEG: "steer",
    SIG_DASH_TURN_L: "turn_l",
    SIG_DASH_TURN_R: "turn_r",
    SIG_DASH_WIPER: "wiper",
    SIG_DASH_HIGH_BEAM: "high_beam",
}


def walk_tlv(payload: bytes) -> List[Tuple[int, int, int, int]]:
    """Return (id, type, qflags, raw) for each TLV: id|type|qflags|raw[W]."""
    out: List[Tuple[int, int, int, int]] = []
    i = 0
    while i + 4 <= len(payload):
        sid = payload[i] | (payload[i + 1] << 8)
        typ = payload[i + 2]
        qflags = payload[i + 3]
        width = WIDTH.get(typ)
        if width is None:
            break
        if i + 4 + width > len(payload):
            break
        raw = int.from_bytes(payload[i + 4 : i + 4 + width], "little", signed=False)
        out.append((sid, typ, qflags, raw))
        i += 4 + width
    return out


def add_demo_rows(dash: Dashboard) -> None:
    dash.add_section("PT / CAN0")
    dash.add("rpm", "RPM", "rpm", 0, 7000)
    dash.add("coolant", "Coolant", "C", -40, 120)
    dash.add("torque", "Torque", "Nm", -50, 400)
    dash.add_section("UDS PID / ext")
    dash.add("boost", "Boost", "bar", 0, 3)
    dash.add("fuel", "Fuel P", "kPa", 0, 600)
    dash.add_section("Dash / CAN1 Tier B")
    dash.add("steer", "Steer", "deg", -90, 90)
    dash.add("turn_l", "Turn L", "", 0, 1)
    dash.add("turn_r", "Turn R", "", 0, 1)
    dash.add("wiper", "Wiper", "", 0, 1)
    dash.add("high_beam", "Hi beam", "", 0, 1)


class TlmFeed:
    def __init__(self, session: UcpSession, dash: Dashboard) -> None:
        self.session = session
        self.dash = dash
        self.rx_ok = False
        self.generation = 0
        self._pkt_counts = {p: 0 for p in TLM_LIVE_PKTS}
        self._rate_window_t0 = 0.0
        self._rate_window_counts = {p: 0 for p in TLM_LIVE_PKTS}
        self._hz = {p: 0.0 for p in TLM_LIVE_PKTS}
        self._last_pkt_t = {p: 0.0 for p in TLM_LIVE_PKTS}
        self._last_gap_ms = {p: 0.0 for p in TLM_LIVE_PKTS}
        session.register_app(APP_TLM, self.on_tlm)

    def on_tlm(self, pkt: int, payload: bytes) -> None:
        import time

        now = time.monotonic()
        if pkt not in TLM_LIVE_PKTS or len(payload) < 3:
            return
        self.rx_ok = True
        self.dash.touch_link(now)
        self.generation = payload[0]
        prev = self._last_pkt_t.get(pkt, 0.0)
        if prev > 0.0:
            self._last_gap_ms[pkt] = (now - prev) * 1000.0
        self._last_pkt_t[pkt] = now
        self._pkt_counts[pkt] = self._pkt_counts.get(pkt, 0) + 1
        if self._rate_window_t0 <= 0.0:
            self._rate_window_t0 = now
        self._rate_window_counts[pkt] = self._rate_window_counts.get(pkt, 0) + 1
        elapsed = now - self._rate_window_t0
        if elapsed >= 1.0:
            for p in TLM_LIVE_PKTS:
                self._hz[p] = self._rate_window_counts.get(p, 0) / elapsed
                self._rate_window_counts[p] = 0
            self._rate_window_t0 = now
        # Multipart header: gen | part_index | part_count | TLVs…
        for sid, typ, qflags, raw in walk_tlv(payload[3:]):
            key = DASH_KEYS.get(sid)
            if key is None:
                continue
            data_ok = (qflags & SIG_TLV_Q_VALID) != 0
            eng = decode_eng(sid, raw, typ)
            if eng is not None:
                self.dash.set(key, float(eng), data_ok=data_ok)
        self.refresh_status()

    def refresh_status(self) -> None:
        link = "READY" if self.session.ready else "discovering"
        rx = "RX OK" if self.rx_ok else "RX —"
        hz = (
            f"L={self._hz.get(PKT_LIVE, 0):.0f}/{self._last_gap_ms.get(PKT_LIVE, 0):.0f}ms "
            f"H={self._hz.get(PKT_HF, 0):.0f}/{self._last_gap_ms.get(PKT_HF, 0):.0f}ms "
            f"M={self._hz.get(PKT_MF, 0):.0f} "
            f"F={self._hz.get(PKT_LF, 0):.0f} Hz"
        )
        self.dash.set_status(f"{link}  |  {rx}  |  gen={self.generation}  |  {hz}")

    def on_session_not_ready(self) -> None:
        self.rx_ok = False
        self.refresh_status()
