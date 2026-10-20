#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Demo TUI: shared UCP session (TLM + CFG), bar dashboard, CAN worker toggles.

Requires:
  pip install textual pyserial python-can

Example:
  python tools/demo_tui.py --port COM7
"""

from __future__ import annotations

import argparse
import sys
import threading
import time
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial is required: pip install pyserial") from exc

try:
    from textual import on, work
    from textual.app import App, ComposeResult
    from textual.containers import Horizontal, Vertical, VerticalScroll
    from textual.reactive import reactive
    from textual.widgets import (
        Button,
        Checkbox,
        Footer,
        Header,
        Input,
        Label,
        Select,
        Static,
        Switch,
    )
except ImportError as exc:  # pragma: no cover
    raise SystemExit("textual is required: pip install textual") from exc

from aep_demo_pt import (  # noqa: E402
    COMP_ID_CLUSTER,
    COMP_ID_DASH,
    COMP_ID_PID_CLIENT,
    COMP_ID_PT_PARSER,
)
from demo_workers import WorkerSupervisor  # noqa: E402
from term_dashboard import Dashboard  # noqa: E402
from ucp_cfg_client import CfgServiceClient, pt_profile_from_cfg, slot_by_id  # noqa: E402
from ucp_session import UcpSession, cap_tlm_and_cfg  # noqa: E402
from ucp_tlm_feed import TlmFeed, add_demo_rows  # noqa: E402


class DashPanel(Static):
    """Renders term_dashboard lines into a Textual widget (skip unchanged frames)."""

    def __init__(self, dash: Dashboard, **kwargs) -> None:
        super().__init__(**kwargs)
        self.dash = dash
        self._last_key: object = None

    def refresh_dash(self) -> None:
        key = self.dash.content_key()
        if key == self._last_key:
            return
        self._last_key = key
        self.update(self.dash.render_text())


class DemoTui(App):
    TITLE = "AEP Demo TUI"
    CSS = """
    Screen { layout: vertical; }
    #top { height: auto; dock: top; }
    #main { height: 1fr; }
    #can_panel { width: 34; border: solid $primary; }
    #dash_wrap { width: 1fr; border: solid $accent; }
    #cfg { width: 38; border: solid $warning; }
    #worker_help, #cfg_help { color: $text-muted; margin: 1 1; }
    #status { height: 3; dock: bottom; border-top: solid $primary; }
    .row { height: auto; margin: 0 1; }
    Switch { width: auto; }
    """

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "connect", "Connect"),
        ("d", "disconnect", "Disconnect"),
        ("r", "refresh_slots", "Refresh cfg"),
    ]

    link_status: reactive[str] = reactive("disconnected")
    cfg_status: reactive[str] = reactive("—")

    def __init__(self, port: str = "", baud: int = 115200) -> None:
        super().__init__()
        self._port_default = port
        self._baud = baud
        self._ser: Optional[serial.Serial] = None
        self._session: Optional[UcpSession] = None
        self._cfg: Optional[CfgServiceClient] = None
        self._feed: Optional[TlmFeed] = None
        self._io_stop = threading.Event()
        self._io_thread: Optional[threading.Thread] = None
        self._dash = Dashboard(
            title="Board UCP (USB) — sig table",
            stale_s=2.5,
            bar_width=18,
        )
        add_demo_rows(self._dash)
        # Must not use `_workers` — Textual App stores its WorkerManager there.
        self._can_sup = WorkerSupervisor()
        self._pt_switch_guard = False

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="top"):
            yield Label("Port")
            yield Input(value=self._port_default, placeholder="COM7", id="port")
            yield Button("Ports", id="btn_ports")
            yield Button("Connect", id="btn_connect", variant="success")
            yield Button("Disconnect", id="btn_disconnect")
            yield Label("CAN if")
            yield Input(value="kvaser", id="can_if", classes="row")
            yield Label("CH0")
            yield Input(value="0", id="ch0", classes="row")
            yield Label("CH1")
            yield Input(value="1", id="ch1", classes="row")
        with Horizontal(id="main"):
            with Vertical(id="can_panel"):
                yield Label("CAN workers (host PC)")
                yield Horizontal(
                    Switch(id="sw_pt_a"),
                    Label("PT-A → CAN0 0x201/202"),
                    classes="row",
                )
                yield Horizontal(
                    Switch(id="sw_pt_b"),
                    Label("PT-B → CAN0 0x211/212"),
                    classes="row",
                )
                yield Horizontal(
                    Switch(id="sw_uds"),
                    Label("UDS PID ECU → CAN0"),
                    classes="row",
                )
                yield Horizontal(
                    Switch(id="sw_dash"),
                    Label("Dash gen → CAN1 0x401/402"),
                    classes="row",
                )
                yield Horizontal(
                    Switch(id="sw_sniff"),
                    Label("Cluster sniff CAN1"),
                    classes="row",
                )
                yield Static("", id="worker_pids")
                yield Static(
                    "PT-A/B exclusive. Match board PT profile.\n"
                    "Dash gen needs Dash checkbox + Apply.\n"
                    "Middle bars = USB UCP from board.",
                    id="worker_help",
                )
            with Vertical(id="dash_wrap"):
                yield Label("Board UCP telemetry (USB APP 0x20)")
                yield DashPanel(self._dash, id="dash")
            with VerticalScroll(id="cfg"):
                yield Label("Board config (APP 0x71)")
                yield Checkbox("PT parser (CAN0 RX)", value=True, id="cb_pt")
                yield Checkbox("PID client (UDS)", value=True, id="cb_pid")
                yield Checkbox("Cluster TX 0x301 (CAN1)", value=True, id="cb_cluster")
                yield Checkbox("Dash RX 0x401/402 (CAN1)", value=False, id="cb_dash")
                yield Static(
                    "Cluster = board publishes RPM/…\n"
                    "on CAN1 for a real cluster.\n"
                    "Sniff watches that bus only.\n"
                    "Dash = stalk/steer input pack.",
                    id="cfg_help",
                )
                yield Label("PT profile")
                yield Select(
                    [("A (0x201/202)", "a"), ("B (0x211/212)", "b")],
                    value="a",
                    id="sel_profile",
                )
                yield Button("Refresh slots", id="btn_refresh")
                yield Button("Apply", id="btn_apply", variant="primary")
                yield Button("Save NVS", id="btn_save")
                yield Static("cfg: —", id="cfg_msg")
        yield Static("status: disconnected", id="status")
        yield Footer()

    def on_mount(self) -> None:
        self.set_interval(0.1, self._ui_tick)
        self._last_worker_line = ""
        self._last_status_line = ""

    def _ui_tick(self) -> None:
        self._can_sup.poll()
        wline = self._can_sup.status_line()
        if wline != self._last_worker_line:
            self._last_worker_line = wline
            self.query_one("#worker_pids", Static).update(wline)
        self.query_one("#dash", DashPanel).refresh_dash()
        if self._session is not None and self._feed is not None:
            age = (
                time.monotonic() - self._dash.link_rx_stamp
                if self._dash.link_rx_stamp
                else 99.0
            )
            if age > 2.5:
                self._feed.rx_ok = False
                self._feed.refresh_status()
            link = "READY" if self._session.ready else "discovering"
            rx = "RX OK" if self._feed.rx_ok else "RX —"
            sline = (
                f"link={link}  {rx}  gen={self._feed.generation}  |  cfg: {self.cfg_status}"
            )
        else:
            sline = f"status: disconnected  |  cfg: {self.cfg_status}"
        if sline != self._last_status_line:
            self._last_status_line = sline
            self.query_one("#status", Static).update(sline)

    def _apply_worker_settings(self) -> None:
        self._can_sup.interface = self.query_one("#can_if", Input).value.strip() or "kvaser"
        self._can_sup.ch0 = self.query_one("#ch0", Input).value.strip() or "0"
        self._can_sup.ch1 = self.query_one("#ch1", Input).value.strip() or "1"

    def _set_pt_switch(self, switch_id: str, value: bool) -> None:
        self._pt_switch_guard = True
        try:
            self.query_one(switch_id, Switch).value = value
        finally:
            self._pt_switch_guard = False

    @on(Switch.Changed, "#sw_pt_a")
    def _sw_pt_a(self, event: Switch.Changed) -> None:
        if self._pt_switch_guard:
            return
        self._apply_worker_settings()
        if event.value:
            self._set_pt_switch("#sw_pt_b", False)
        self._can_sup.set_running("pt_a", event.value)

    @on(Switch.Changed, "#sw_pt_b")
    def _sw_pt_b(self, event: Switch.Changed) -> None:
        if self._pt_switch_guard:
            return
        self._apply_worker_settings()
        if event.value:
            self._set_pt_switch("#sw_pt_a", False)
        self._can_sup.set_running("pt_b", event.value)

    @on(Switch.Changed, "#sw_uds")
    def _sw_uds(self, event: Switch.Changed) -> None:
        self._apply_worker_settings()
        self._can_sup.set_running("uds", event.value)

    @on(Switch.Changed, "#sw_dash")
    def _sw_dash(self, event: Switch.Changed) -> None:
        self._apply_worker_settings()
        self._can_sup.set_running("dash", event.value)
        if event.value:
            self._set_cfg_msg("enable Dash + Apply if steer/stalks stay empty")

    @on(Switch.Changed, "#sw_sniff")
    def _sw_sniff(self, event: Switch.Changed) -> None:
        self._apply_worker_settings()
        self._can_sup.set_running("sniff", event.value)

    @on(Button.Pressed, "#btn_ports")
    def _list_ports(self) -> None:
        ports = [p.device for p in list_ports.comports()]
        self.cfg_status = "ports: " + (", ".join(ports) if ports else "(none)")
        self.query_one("#cfg_msg", Static).update(f"cfg: {self.cfg_status}")

    @on(Button.Pressed, "#btn_connect")
    def _btn_connect(self) -> None:
        self.action_connect()

    @on(Button.Pressed, "#btn_disconnect")
    def _btn_disconnect(self) -> None:
        self.action_disconnect()

    def action_connect(self) -> None:
        if self._session is not None:
            self.cfg_status = "already connected"
            return
        port = self.query_one("#port", Input).value.strip()
        if not port:
            self.cfg_status = "set COM port"
            self.query_one("#cfg_msg", Static).update(f"cfg: {self.cfg_status}")
            return
        try:
            # timeout=0: hot path drains in_waiting; drain_once() arms ReadFile briefly.
            ser = serial.Serial(port, self._baud, timeout=0)
            try:
                ser.dtr = True
                ser.rts = True
            except Exception:
                pass
            try:
                # Larger driver buffer; ignore if unsupported.
                ser.set_buffer_size(rx_size=65536, tx_size=65536)
            except Exception:
                pass
        except Exception as exc:
            self.cfg_status = f"open failed: {exc}"
            self.query_one("#cfg_msg", Static).update(f"cfg: {self.cfg_status}")
            return

        self._ser = ser
        self._session = UcpSession(ser, cap_tlm_and_cfg())
        self._dash.title = f"Board UCP (USB)  —  {port}"
        self._feed = TlmFeed(self._session, self._dash)
        self._cfg = CfgServiceClient(self._session, auto_pump=False)
        self._session.set_on_ready(self._feed.refresh_status)
        self._session.set_on_not_ready(self._feed.on_session_not_ready)
        self._feed.refresh_status()

        self._io_stop.clear()
        self._io_thread = threading.Thread(target=self._io_loop, name="ucp-io", daemon=True)
        self._io_thread.start()
        self.cfg_status = "connecting…"
        self.query_one("#cfg_msg", Static).update(f"cfg: {self.cfg_status}")
        self._wait_and_refresh_slots()

    def action_disconnect(self) -> None:
        self._io_stop.set()
        if self._io_thread is not None:
            self._io_thread.join(timeout=2.0)
            self._io_thread = None
        self._cfg = None
        self._feed = None
        self._session = None
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None
        self.cfg_status = "disconnected"
        self.query_one("#cfg_msg", Static).update(f"cfg: {self.cfg_status}")

    def _io_loop(self) -> None:
        while not self._io_stop.is_set():
            sess = self._session
            if sess is None:
                break
            try:
                sess.tick()
                sess.drain_available()
                chunk = sess.drain_once(arm_s=0.002)
                if chunk:
                    sess.pump(chunk)
                    sess.drain_available()
            except Exception:
                break

    @work(thread=True)
    def _wait_and_refresh_slots(self) -> None:
        sess = self._session
        cfg = self._cfg
        if sess is None or cfg is None:
            return
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline and not sess.ready:
            time.sleep(0.05)
        if not sess.ready:
            self.call_from_thread(self._set_cfg_msg, "link timeout")
            return
        try:
            slots = cfg.get_slots()
            self.call_from_thread(self._apply_slots_to_ui, slots)
            self.call_from_thread(self._set_cfg_msg, "slots loaded")
        except Exception as exc:
            self.call_from_thread(self._set_cfg_msg, f"slots err: {exc}")

    def _set_cfg_msg(self, msg: str) -> None:
        self.cfg_status = msg
        self.query_one("#cfg_msg", Static).update(f"cfg: {msg}")

    def _apply_slots_to_ui(self, slots) -> None:
        def en(cid: int, default: bool) -> bool:
            s = slot_by_id(slots, cid)
            return s.enabled if s is not None else default

        self.query_one("#cb_pt", Checkbox).value = en(COMP_ID_PT_PARSER, True)
        self.query_one("#cb_pid", Checkbox).value = en(COMP_ID_PID_CLIENT, True)
        self.query_one("#cb_cluster", Checkbox).value = en(COMP_ID_CLUSTER, True)
        self.query_one("#cb_dash", Checkbox).value = en(COMP_ID_DASH, False)
        pt = slot_by_id(slots, COMP_ID_PT_PARSER)
        if pt is not None:
            self.query_one("#sel_profile", Select).value = pt_profile_from_cfg(pt.cfg)

    @on(Button.Pressed, "#btn_refresh")
    def _btn_refresh(self) -> None:
        self.action_refresh_slots()

    def action_refresh_slots(self) -> None:
        self._refresh_slots_work()

    @work(thread=True)
    def _refresh_slots_work(self) -> None:
        cfg = self._cfg
        if cfg is None:
            self.call_from_thread(self._set_cfg_msg, "not connected")
            return
        try:
            slots = cfg.get_slots()
            self.call_from_thread(self._apply_slots_to_ui, slots)
            self.call_from_thread(self._set_cfg_msg, "slots refreshed")
        except Exception as exc:
            self.call_from_thread(self._set_cfg_msg, f"refresh err: {exc}")

    @on(Button.Pressed, "#btn_apply")
    def _btn_apply(self) -> None:
        pt_en = self.query_one("#cb_pt", Checkbox).value
        pid_en = self.query_one("#cb_pid", Checkbox).value
        cl_en = self.query_one("#cb_cluster", Checkbox).value
        dash_en = self.query_one("#cb_dash", Checkbox).value
        profile = self.query_one("#sel_profile", Select).value
        if profile not in ("a", "b"):
            profile = "a"
        self._apply_work(pt_en, pid_en, cl_en, dash_en, str(profile))

    @work(thread=True)
    def _apply_work(
        self,
        pt_en: bool,
        pid_en: bool,
        cl_en: bool,
        dash_en: bool,
        profile: str,
    ) -> None:
        cfg = self._cfg
        if cfg is None:
            self.call_from_thread(self._set_cfg_msg, "not connected")
            return
        try:
            cfg.set_pt(pt_en, "b" if profile == "b" else "a")
            cfg.set_bus_comp("pid", pid_en)
            cfg.set_bus_comp("cluster", cl_en)
            cfg.set_bus_comp("dash", dash_en)
            cfg.apply()
            slots = cfg.get_slots()
            self.call_from_thread(self._apply_slots_to_ui, slots)
            self.call_from_thread(self._set_cfg_msg, "applied OK")
        except Exception as exc:
            self.call_from_thread(self._set_cfg_msg, f"apply err: {exc}")

    @on(Button.Pressed, "#btn_save")
    def _btn_save(self) -> None:
        self._save_work()

    @work(thread=True)
    def _save_work(self) -> None:
        cfg = self._cfg
        if cfg is None:
            self.call_from_thread(self._set_cfg_msg, "not connected")
            return
        try:
            cfg.save()
            self.call_from_thread(self._set_cfg_msg, "saved (RAM NVS)")
        except Exception as exc:
            self.call_from_thread(self._set_cfg_msg, f"save err: {exc}")

    def action_quit(self) -> None:
        self.action_disconnect()
        self._can_sup.stop_all()
        self.exit()

    def on_unmount(self) -> None:
        self.action_disconnect()
        self._can_sup.stop_all()


def main(argv: Optional[list] = None) -> int:
    ap = argparse.ArgumentParser(description="AEP demo TUI (UCP tlm+cfg + CAN workers)")
    ap.add_argument("--port", default="", help="CDC port, e.g. COM7")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args(argv)
    app = DemoTui(port=args.port, baud=args.baud)
    app.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
