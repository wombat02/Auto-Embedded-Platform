# SPDX-License-Identifier: MIT
"""Host client for UCP transport transfer channel (APP 0x01) — pull + push."""

from __future__ import annotations

import struct
import time
from typing import Optional, Tuple

from ucp_session import UcpSession, frame

APP_XFER = 0x01

PKT_INFO_REQ = 0x01
PKT_INFO_RSP = 0x02
PKT_CHUNK_REQ = 0x03
PKT_CHUNK_RSP = 0x04
PKT_ABORT = 0x05
PKT_PUSH_DATA = 0x10
PKT_PUSH_ACK = 0x11

FLAG_FIRST = 0x01
FLAG_CONT = 0x02
FLAG_LAST = 0x04

ACK_OK = 0
PUSH_WINDOW = 4
PUSH_DATA_CAP = 64 - 5  # token+seq+flags
CHUNK_HDR = 4

SVC_FLAG_OFFERED = 0x01


class UcpXferClient:
    def __init__(self, session: UcpSession) -> None:
        self.session = session
        self._info: Optional[Tuple[int, int, int, int]] = None  # token, total, cap, chunks
        self._chunks: dict[int, bytes] = {}
        self._push_rx: bytearray = bytearray()
        self._push_token = 0
        self._push_expect = 0
        self._push_since_ack = 0
        self._push_complete: Optional[bytes] = None
        self._ack_q: list[Tuple[int, int, int]] = []  # token, seq, status
        session.register_app(APP_XFER, self._on_xfer)

    def _on_xfer(self, pkt: int, payload: bytes) -> None:
        if pkt == PKT_INFO_RSP and len(payload) >= 7:
            token, total, cap, chunks, _flags = struct.unpack_from("<HHBBB", payload, 0)
            self._info = (token, total, cap, chunks)
        elif pkt == PKT_CHUNK_RSP and len(payload) >= 4:
            token, index, _cc = struct.unpack_from("<HBB", payload, 0)
            self._chunks[index] = payload[4:]
        elif pkt == PKT_PUSH_DATA and len(payload) >= 5:
            self._handle_push_data(payload)
        elif pkt == PKT_PUSH_ACK and len(payload) >= 5:
            token, seq, status = struct.unpack_from("<HHB", payload, 0)
            self._ack_q.append((token, seq, status))

    def _handle_push_data(self, payload: bytes) -> None:
        token, seq, flags = struct.unpack_from("<HHB", payload, 0)
        data = payload[5:]
        is_first = bool(flags & FLAG_FIRST)
        is_last = bool(flags & FLAG_LAST)
        if is_first:
            self._push_rx = bytearray()
            self._push_token = token
            self._push_expect = 0
            self._push_since_ack = 0
        elif token != self._push_token:
            self._send_ack(token, 0, 3)  # BAD_SEQ
            return
        if seq != self._push_expect:
            self._send_ack(token, self._push_expect, 3)
            self._push_rx = bytearray()
            return
        self._push_rx.extend(data)
        self._push_expect = seq + 1
        self._push_since_ack += 1
        if is_last or self._push_since_ack >= PUSH_WINDOW:
            self._send_ack(token, seq, ACK_OK)
            self._push_since_ack = 0
        if is_last:
            self._push_complete = bytes(self._push_rx)

    def _send(self, pkt: int, payload: bytes) -> None:
        self.session.ser.write(frame(APP_XFER, pkt, payload))

    def _send_ack(self, token: int, seq: int, status: int) -> None:
        self._send(PKT_PUSH_ACK, struct.pack("<HHB", token, seq, status))

    def pull(self, token: int, timeout: float = 2.0) -> bytes:
        """Pull an offered SDU by token."""
        self._info = None
        self._chunks.clear()
        self._send(PKT_INFO_REQ, struct.pack("<H", token))
        deadline = time.monotonic() + timeout
        while self._info is None and time.monotonic() < deadline:
            self.session.poll(do_tick=False)
            time.sleep(0.001)
        if self._info is None or self._info[0] != token:
            raise TimeoutError("xfer INFO_RSP timeout")
        _t, total, cap, chunks = self._info
        for i in range(chunks):
            self._send(PKT_CHUNK_REQ, struct.pack("<HB", token, i))
            while i not in self._chunks and time.monotonic() < deadline:
                self.session.poll(do_tick=False)
                time.sleep(0.001)
            if i not in self._chunks:
                raise TimeoutError(f"xfer CHUNK_RSP {i} timeout")
        out = b"".join(self._chunks[i] for i in range(chunks))
        if len(out) != total:
            raise ValueError(f"xfer length mismatch got={len(out)} want={total}")
        return out

    def push(self, data: bytes, timeout: float = 5.0) -> int:
        """Push an SDU to the peer; returns token. Peer must reassemble/ACK."""
        if len(data) > 1024:
            raise ValueError("SDU too large")
        token = int(time.monotonic() * 1000) & 0xFFFF
        if token == 0:
            token = 1
        self._ack_q.clear()
        offset = 0
        seq = 0
        deadline = time.monotonic() + timeout
        while True:
            window = 0
            last_seq = seq
            while window < PUSH_WINDOW and (offset < len(data) or (len(data) == 0 and seq == 0)):
                remain = len(data) - offset
                n = min(PUSH_DATA_CAP, remain)
                flags = 0
                if seq == 0:
                    flags |= FLAG_FIRST
                is_last = (offset + n) >= len(data)
                if is_last:
                    flags |= FLAG_LAST
                else:
                    flags |= FLAG_CONT
                payload = struct.pack("<HHB", token, seq, flags) + data[offset : offset + n]
                self._send(PKT_PUSH_DATA, payload)
                last_seq = seq
                seq += 1
                offset += n
                window += 1
                if is_last or window >= PUSH_WINDOW:
                    break
                if len(data) == 0:
                    break
            # wait ACK
            got = False
            while time.monotonic() < deadline:
                self.session.poll(do_tick=False)
                while self._ack_q:
                    t, s, st = self._ack_q.pop(0)
                    if t == token and st == ACK_OK and s >= last_seq:
                        got = True
                        break
                    if t == token and st != ACK_OK:
                        raise RuntimeError(f"push ACK status={st}")
                if got:
                    break
                time.sleep(0.001)
            if not got:
                raise TimeoutError("push ACK timeout")
            if offset >= len(data):
                return token

    def take_push_rx(self) -> Optional[bytes]:
        """If this host received a complete push SDU, return and clear it."""
        out = self._push_complete
        self._push_complete = None
        return out


def parse_offered_rsp_body(body: bytes) -> Tuple[int, int]:
    """Parse service RSP body after status: flag, token, total_len → (token, total_len)."""
    if len(body) < 5 or body[0] != SVC_FLAG_OFFERED:
        raise ValueError("not an offered xfer RSP body")
    token, total = struct.unpack_from("<HH", body, 1)
    return token, total
