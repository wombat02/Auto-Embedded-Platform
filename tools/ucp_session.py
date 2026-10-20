# SPDX-License-Identifier: MIT
"""Shared UCP V1 framing + control-plane session (HELLO / CAP / LINK_READY)."""

from __future__ import annotations

import struct
import time
from typing import Callable, List, Optional, Sequence, Tuple

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial is required: pip install pyserial") from exc

SOF = 0xA5
APP_CONTROL = 0x00
APP_TLM = 0x20
APP_CFG = 0x71
APP_VER = 0x0100  # V1.0 — high byte major, low byte minor
UCP_PROTO = 0x0100

PKT_HELLO_REQ = 0x00
PKT_HELLO_RSP = 0x01
PKT_CAP_REQ = 0x02
PKT_CAP_RSP = 0x03
PKT_KEEPALIVE = 0x06
PKT_LINK_READY_REQ = 0x07
PKT_LINK_READY_RSP = 0x08

PKT_REQ = 0xFA
PKT_RSP = 0xFB

APP_XFER = 0x01

PKT_LIVE = 0x10
PKT_HF = 0x11
PKT_MF = 0x12
PKT_LF = 0x13
PKT_CHANGE = 0x14

ROLE_TX = 1 << 0
ROLE_RX = 1 << 1
ROLE_REQ_RSP = 1 << 2

HW_HOST = 0x80

# CapEntry = (app_id, version, role_flags)
CapEntry = Tuple[int, int, int]

AppHandler = Callable[[int, bytes], None]


def checksum(data: bytes) -> int:
    cs = 0
    for b in data:
        cs ^= b
    return cs & 0xFF


def frame(app_id: int, pkt_id: int, payload: bytes = b"") -> bytes:
    if len(payload) > 64:
        raise ValueError("payload too long")
    body = bytes([app_id, pkt_id, len(payload)]) + payload
    return bytes([SOF]) + body + bytes([checksum(body)])


class Parser:
    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.state = "sof"
        self.app = 0
        self.pkt = 0
        self.length = 0
        self.idx = 0
        self.cs = 0
        self.payload = bytearray()

    def feed(self, byte: int) -> Optional[Tuple[int, int, bytes]]:
        if self.state == "sof":
            if byte == SOF:
                self.cs = 0
                self.state = "app"
            return None
        if self.state == "app":
            self.app = byte
            self.cs ^= byte
            self.state = "pkt"
            return None
        if self.state == "pkt":
            self.pkt = byte
            self.cs ^= byte
            self.state = "len"
            return None
        if self.state == "len":
            self.length = byte
            self.cs ^= byte
            self.idx = 0
            self.payload = bytearray()
            self.state = "sum" if self.length == 0 else "payload"
            return None
        if self.state == "payload":
            self.payload.append(byte)
            self.cs ^= byte
            self.idx += 1
            if self.idx >= self.length:
                self.state = "sum"
            return None
        if self.state == "sum":
            ok = byte == self.cs
            app, pkt, payload = self.app, self.pkt, bytes(self.payload)
            self.reset()
            if ok:
                return app, pkt, payload
            return None
        self.reset()
        return None


def cap_tlm_only() -> List[CapEntry]:
    return [(APP_TLM, APP_VER, ROLE_RX | ROLE_REQ_RSP)]


def cap_cfg_only() -> List[CapEntry]:
    return [(APP_CFG, APP_VER, ROLE_REQ_RSP)]


def cap_tlm_and_cfg() -> List[CapEntry]:
    return [
        (APP_TLM, APP_VER, ROLE_RX | ROLE_REQ_RSP),
        (APP_CFG, APP_VER, ROLE_REQ_RSP),
    ]


class UcpSession:
    """Owns serial + control plane; dispatches app frames to registered handlers."""

    def __init__(
        self,
        ser: serial.Serial,
        cap_entries: Sequence[CapEntry],
    ) -> None:
        self.ser = ser
        self.cap_entries: List[CapEntry] = list(cap_entries)
        self.parser = Parser()
        self.nonce = 1
        self.ready = False
        self.last_rx = time.monotonic()
        self.last_hello = 0.0
        self.last_keepalive = 0.0
        self._app_handlers: dict[int, AppHandler] = {}
        self._on_ready: Optional[Callable[[], None]] = None
        self._on_not_ready: Optional[Callable[[], None]] = None

    def set_on_ready(self, cb: Optional[Callable[[], None]]) -> None:
        self._on_ready = cb

    def set_on_not_ready(self, cb: Optional[Callable[[], None]]) -> None:
        self._on_not_ready = cb

    def register_app(self, app_id: int, handler: AppHandler) -> None:
        self._app_handlers[app_id] = handler

    def send(self, app: int, pkt: int, payload: bytes = b"") -> None:
        self.ser.write(frame(app, pkt, payload))

    def send_hello(self, pkt: int) -> None:
        self.send(APP_CONTROL, pkt, struct.pack("<BHH", HW_HOST, UCP_PROTO, self.nonce))

    def send_cap(self, pkt: int) -> None:
        entries = b"".join(
            struct.pack("<BHB", app_id, ver, roles) for app_id, ver, roles in self.cap_entries
        )
        payload = struct.pack("<HHB", UCP_PROTO, self.nonce, len(self.cap_entries)) + entries
        self.send(APP_CONTROL, pkt, payload)

    def send_link_ready(self, pkt: int) -> None:
        self.send(APP_CONTROL, pkt, struct.pack("<BHH", 0, UCP_PROTO, self.nonce))

    def _set_ready(self, ready: bool) -> None:
        was = self.ready
        self.ready = ready
        if ready and not was and self._on_ready is not None:
            self._on_ready()
        if not ready and was and self._on_not_ready is not None:
            self._on_not_ready()

    def on_control(self, pkt: int, payload: bytes) -> None:
        self.last_rx = time.monotonic()
        if pkt == PKT_HELLO_REQ:
            if len(payload) >= 5:
                self.send_hello(PKT_HELLO_RSP)
                self.send_cap(PKT_CAP_REQ)
        elif pkt == PKT_HELLO_RSP:
            self.send_cap(PKT_CAP_REQ)
        elif pkt == PKT_CAP_REQ:
            self.send_cap(PKT_CAP_RSP)
            self.send_link_ready(PKT_LINK_READY_REQ)
        elif pkt == PKT_CAP_RSP:
            self.send_link_ready(PKT_LINK_READY_REQ)
        elif pkt == PKT_LINK_READY_REQ:
            self.send_link_ready(PKT_LINK_READY_RSP)
            self._set_ready(True)
        elif pkt == PKT_LINK_READY_RSP:
            self._set_ready(True)
        elif pkt == PKT_KEEPALIVE:
            pass

    def pump(self, data: bytes) -> None:
        for b in data:
            got = self.parser.feed(b)
            if not got:
                continue
            app, pkt, payload = got
            if app == APP_CONTROL:
                self.on_control(pkt, payload)
                continue
            handler = self._app_handlers.get(app)
            if handler is not None:
                self.last_rx = time.monotonic()
                handler(pkt, payload)

    def tick(self) -> None:
        now = time.monotonic()
        if not self.ready and (now - self.last_hello) >= 0.5:
            self.last_hello = now
            self.send_hello(PKT_HELLO_REQ)
        # Host OUT still helps Windows usbser complete device bulk-IN. With
        # forever-armed device OUT, 50 ms is enough for a stable link test.
        if self.ready and (now - self.last_keepalive) >= 0.050:
            self.last_keepalive = now
            self.send(APP_CONTROL, PKT_KEEPALIVE, b"")
        if self.ready and (now - self.last_rx) > 3.0:
            self._set_ready(False)

    def wait_ready(self, timeout: float = 5.0) -> None:
        deadline = time.time() + timeout
        self.send_hello(PKT_HELLO_REQ)
        while time.time() < deadline and not self.ready:
            self.tick()
            self.drain_available()
            chunk = self.drain_once(arm_s=0.02)
            if chunk:
                self.pump(chunk)
                self.drain_available()
            elif not self.ready:
                self.send_hello(PKT_HELLO_REQ)
        if not self.ready:
            raise TimeoutError("UCP link not ready")

    def drain_available(self) -> int:
        """Non-blocking drain of driver RX buffer. Returns bytes pumped."""
        total = 0
        while True:
            waiting = getattr(self.ser, "in_waiting", 0) or 0
            if waiting <= 0:
                break
            chunk = self.ser.read(waiting)
            if not chunk:
                break
            self.pump(chunk)
            total += len(chunk)
        return total

    def drain_once(self, *, arm_s: float = 0.002) -> bytes:
        """Issue a short ReadFile (arms bulk-IN), then return any bytes read.

        Prefer drain_available() in a hot loop; use this when the buffer is empty
        so the device’s blocking cdc_acm_write can complete.
        """
        old = getattr(self.ser, "timeout", None)
        try:
            self.ser.timeout = arm_s
            return self.ser.read(2048)
        finally:
            if old is not None:
                self.ser.timeout = old

    def poll(self, *, do_tick: bool = True) -> None:
        """Keepalive OUT, drain buffered IN, then briefly arm ReadFile."""
        if do_tick:
            self.tick()
        self.drain_available()
        chunk = self.drain_once()
        if chunk:
            self.pump(chunk)
            self.drain_available()
