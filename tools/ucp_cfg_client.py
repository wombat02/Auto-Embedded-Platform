# SPDX-License-Identifier: MIT
"""UCP APP 0x71 config service client — rides an existing UcpSession."""

from __future__ import annotations

import struct
import threading
import time
from concurrent.futures import Future
from dataclasses import dataclass
from typing import List, Literal, Optional, Tuple

from aep_demo_pt import (
    COMP_ID_CLUSTER,
    COMP_ID_DASH,
    COMP_ID_PID_CLIENT,
    COMP_ID_PT_PARSER,
    PT_PROFILE_A,
    PT_PROFILE_B,
)
from ucp_session import APP_CFG, PKT_REQ, PKT_RSP, UcpSession

OP_LIST_DESCS = 0x01
OP_GET_SLOTS = 0x02
OP_SET_SLOT = 0x03
OP_APPLY = 0x04
OP_SAVE = 0x05
OP_LOAD = 0x06
OP_GET_ADS = 0x07

ProfileName = Literal["a", "b"]
BusCompName = Literal["pid", "cluster", "dash"]


@dataclass
class Desc:
    comp_id: int
    kind: int
    max_cfg: int
    name: str


@dataclass
class Slot:
    comp_id: int
    enabled: bool
    cfg: bytes


class CfgServiceClient:
    """APP 0x71 REQ/RSP over a shared session (IO pump must call session.pump)."""

    def __init__(self, session: UcpSession, *, auto_pump: bool = True) -> None:
        self.session = session
        self.auto_pump = auto_pump  # False when an IO thread already pumps the session
        self._corr = 1
        self._lock = threading.Lock()
        # corr -> (expected_opcode, future)
        self._pending: dict[int, tuple[int, Future[bytes]]] = {}
        session.register_app(APP_CFG, self._on_app)

    def _on_app(self, pkt: int, payload: bytes) -> None:
        if pkt != PKT_RSP or len(payload) < 4:
            return
        corr, opcode, status = struct.unpack_from("<HBB", payload, 0)
        body = payload[4:]
        with self._lock:
            item = self._pending.get(corr)
        if item is None:
            return
        expect_op, fut = item
        if fut.done():
            return
        if opcode != expect_op:
            return
        if status != 0:
            fut.set_exception(RuntimeError(f"opcode 0x{opcode:02X} status={status}"))
            with self._lock:
                self._pending.pop(corr, None)
            return
        fut.set_result(body)
        with self._lock:
            self._pending.pop(corr, None)

    def request(self, opcode: int, body: bytes = b"", timeout: float = 2.0) -> bytes:
        """Send REQ and wait for matching RSP. Caller must keep pumping the session."""
        with self._lock:
            corr = self._corr & 0xFFFF
            self._corr = (self._corr + 1) & 0xFFFF
            fut: Future[bytes] = Future()
            self._pending[corr] = (opcode, fut)

        payload = struct.pack("<HBB", corr, opcode, 0) + body
        self.session.send(APP_CFG, PKT_REQ, payload)

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if fut.done():
                return fut.result()
            if self.auto_pump:
                chunk = self.session.drain_once()
                if chunk:
                    self.session.pump(chunk)
                else:
                    time.sleep(0.005)
            else:
                time.sleep(0.005)
        with self._lock:
            self._pending.pop(corr, None)
        if not fut.done():
            fut.cancel()
        raise TimeoutError(f"no RSP for opcode 0x{opcode:02X}")

    def list_descs(self) -> List[Desc]:
        body = self.request(OP_LIST_DESCS)
        if not body:
            return []
        n = body[0]
        off = 1
        out: List[Desc] = []
        while len(out) < n and off + 6 <= len(body):
            cid, kind, max_cfg = struct.unpack_from("<HBH", body, off)
            off += 5
            name_len = body[off]
            off += 1
            if off + name_len > len(body):
                break
            name = body[off : off + name_len].decode("ascii", errors="replace")
            off += name_len
            out.append(Desc(cid, kind, max_cfg, name))
        return out

    def get_slots(self) -> List[Slot]:
        body = self.request(OP_GET_SLOTS)
        if not body:
            return []
        n = body[0]
        off = 1
        out: List[Slot] = []
        while len(out) < n and off + 4 <= len(body):
            cid, en, clen = struct.unpack_from("<HBB", body, off)
            off += 4
            if off + clen > len(body):
                break
            cfg = body[off : off + clen]
            off += clen
            out.append(Slot(cid, en != 0, cfg))
        return out

    def set_slot(self, comp_id: int, enabled: bool, cfg: bytes) -> None:
        body = struct.pack("<HBB", comp_id, 1 if enabled else 0, len(cfg)) + cfg
        self.request(OP_SET_SLOT, body)

    def apply(self) -> None:
        self.request(OP_APPLY)

    def save(self) -> None:
        self.request(OP_SAVE)

    def load(self) -> None:
        self.request(OP_LOAD)

    def get_ads(self) -> List[Tuple[int, int]]:
        body = self.request(OP_GET_ADS)
        if not body:
            return []
        n = body[0]
        off = 1
        out: List[Tuple[int, int]] = []
        for _ in range(n):
            if off + 4 > len(body):
                break
            pid, rev = struct.unpack_from("<HH", body, off)
            off += 4
            out.append((pid, rev))
        return out

    def set_pt(self, enabled: bool, profile: ProfileName, bus: int = 0) -> None:
        prof = PT_PROFILE_A if profile == "a" else PT_PROFILE_B
        self.set_slot(COMP_ID_PT_PARSER, enabled, bytes([bus & 0xFF, prof & 0xFF]))

    def set_bus_comp(
        self,
        which: BusCompName,
        enabled: bool,
        bus: Optional[int] = None,
    ) -> None:
        ids = {
            "pid": COMP_ID_PID_CLIENT,
            "cluster": COMP_ID_CLUSTER,
            "dash": COMP_ID_DASH,
        }
        if bus is None:
            bus = 1 if which in ("cluster", "dash") else 0
        self.set_slot(ids[which], enabled, bytes([bus & 0xFF]))


def slot_by_id(slots: List[Slot], comp_id: int) -> Optional[Slot]:
    for s in slots:
        if s.comp_id == comp_id:
            return s
    return None


def pt_profile_from_cfg(cfg: bytes) -> ProfileName:
    if len(cfg) >= 2 and cfg[1] == PT_PROFILE_B:
        return "b"
    return "a"
