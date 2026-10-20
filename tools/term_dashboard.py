# SPDX-License-Identifier: MIT
"""Fixed-frame ANSI terminal dashboard (bar graphs, in-place redraw)."""

from __future__ import annotations

import sys
import time
from dataclasses import dataclass, field
from typing import List, Optional, TextIO, Union


CSI = "\033["


def _enable_windows_vt() -> None:
    if sys.platform != "win32":
        return
    try:
        import ctypes

        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_uint32()
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)) == 0:
            return
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
        kernel32.SetConsoleMode(handle, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    except Exception:
        pass


def _supports_ansi() -> bool:
    if not sys.stdout.isatty():
        return False
    _enable_windows_vt()
    return True


@dataclass
class DashRow:
    key: str
    label: str
    unit: str
    vmin: float
    vmax: float
    value: Optional[float] = None
    stamp: float = 0.0
    seen: bool = False
    data_ok: bool = False

    def set(
        self,
        value: float,
        now: Optional[float] = None,
        *,
        data_ok: bool = True,
    ) -> None:
        self.value = value
        self.stamp = time.monotonic() if now is None else now
        self.seen = True
        self.data_ok = data_ok


@dataclass
class DashSection:
    """Visual separator between signal groups."""

    title: str


DashItem = Union[DashRow, DashSection]


@dataclass
class Dashboard:
    title: str
    stale_s: float = 2.5
    bar_width: int = 28
    stream: TextIO = field(default_factory=lambda: sys.stdout)
    items: List[DashItem] = field(default_factory=list)
    status: str = "starting"
    link_rx_stamp: float = 0.0
    # Cap line width (avoid get_terminal_size every frame — slow on Windows).
    max_cols: int = 96
    _drawn: bool = False
    _ansi: bool = field(default_factory=_supports_ansi)
    _last_lines: int = 0
    _row_by_key: dict = field(default_factory=dict, repr=False)

    @property
    def rows(self) -> List[DashRow]:
        return [i for i in self.items if isinstance(i, DashRow)]

    def add_section(self, title: str) -> None:
        self.items.append(DashSection(title=title))

    def add(
        self,
        key: str,
        label: str,
        unit: str,
        vmin: float,
        vmax: float,
    ) -> None:
        row = DashRow(key=key, label=label, unit=unit, vmin=vmin, vmax=vmax)
        self.items.append(row)
        self._row_by_key[key] = row

    def set(self, key: str, value: float, *, data_ok: bool = True) -> None:
        row = self._row_by_key.get(key)
        if row is not None:
            row.set(value, data_ok=data_ok)

    def touch_link(self, now: Optional[float] = None) -> None:
        self.link_rx_stamp = time.monotonic() if now is None else now

    def set_status(self, status: str) -> None:
        self.status = status

    def _bar(self, row: DashRow) -> str:
        w = max(8, self.bar_width)
        if row.value is None:
            return "[" + ("-" * w) + "]"
        span = row.vmax - row.vmin
        if span <= 0:
            frac = 0.0
        else:
            frac = (row.value - row.vmin) / span
        frac = 0.0 if frac < 0.0 else 1.0 if frac > 1.0 else frac
        filled = int(round(frac * w))
        return "[" + ("=" * filled) + (" " * (w - filled)) + "]"

    def _age_tag(self, row: DashRow, now: float) -> str:
        if not row.seen:
            return "—"
        link_age = now - self.link_rx_stamp if self.link_rx_stamp > 0.0 else 1e9
        sig_age = now - row.stamp
        if link_age > self.stale_s:
            return f"LINK {link_age:.0f}s"
        if not row.data_ok:
            return f"SIG {sig_age:.0f}s"
        if sig_age > self.stale_s:
            return f"HOLD {sig_age:.0f}s"
        return "OK"

    def _fmt_val(self, row: DashRow) -> str:
        if row.value is None:
            return "—"
        v = row.value
        if abs(v) >= 100 or float(v).is_integer():
            return f"{v:.0f}"
        if abs(v) >= 10:
            return f"{v:.1f}"
        return f"{v:.2f}"

    def render_lines(self) -> List[str]:
        now = time.monotonic()
        cols = self.max_cols
        lines = [
            self.title[:cols],
            f"status  {self.status}"[:cols],
        ]
        label_w = 10
        for item in self.items:
            if isinstance(item, DashRow):
                label_w = max(label_w, len(item.label))
        label_w = min(max(label_w, 8), 14)

        for item in self.items:
            if isinstance(item, DashSection):
                lines.append("")
                title = item.title
                pad = max(0, min(cols, 40) - len(title) - 2)
                left = pad // 2
                right = pad - left
                lines.append(f"{'─' * left} {title} {'─' * right}"[:cols])
                continue
            bar = self._bar(item)
            val = self._fmt_val(item)
            tag = self._age_tag(item, now)
            line = f"{bar}  {item.label:<{label_w}}  {val:>7} {item.unit:<4}  {tag}"
            lines.append(line[:cols])
        return lines

    def render_text(self) -> str:
        return "\n".join(self.render_lines())

    def content_key(self) -> tuple:
        """Stable fingerprint for skip-redraw (age tags quantized to 1s)."""
        now = time.monotonic()
        parts: list = [self.title, self.status]
        for item in self.items:
            if isinstance(item, DashSection):
                parts.append(("S", item.title))
                continue
            v = None if item.value is None else round(item.value, 2)
            link_age = int(now - self.link_rx_stamp) if self.link_rx_stamp else 99
            sig_age = int(now - item.stamp) if item.seen else -1
            tag = 0
            if not item.seen:
                tag = 0
            elif link_age > self.stale_s:
                tag = 1
            elif not item.data_ok:
                tag = 2
            elif sig_age > self.stale_s:
                tag = 3
            else:
                tag = 4
            parts.append((item.key, v, item.data_ok, tag, sig_age if tag in (1, 2, 3) else 0))
        return tuple(parts)

    def draw(self) -> None:
        lines = self.render_lines()
        if not self._ansi:
            self.stream.write("---\n")
            self.stream.write("\n".join(lines) + "\n")
            self.stream.flush()
            return
        if not self._drawn:
            self.stream.write(f"{CSI}?25l{CSI}2J{CSI}H")
            self._drawn = True
        else:
            self.stream.write(f"{CSI}H")
        out = []
        for line in lines:
            out.append(line)
            out.append(f"{CSI}K\n")
        for _ in range(max(0, self._last_lines - len(lines))):
            out.append(f"{CSI}K\n")
        self._last_lines = len(lines)
        self.stream.write("".join(out))
        self.stream.flush()

    def close(self) -> None:
        if self._ansi and self._drawn:
            self.stream.write(f"{CSI}?25h\n")
            self.stream.flush()
