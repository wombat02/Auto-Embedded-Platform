# SPDX-License-Identifier: MIT
"""Supervise demo CAN host tools (PT-A/B gen, UDS PID ECU, cluster sniff)."""

from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set

TOOLS_DIR = Path(__file__).resolve().parent
ROOT = TOOLS_DIR.parent

# Only one PT profile generator at a time (same CAN0, different IDs).
PT_KEYS: Set[str] = {"pt_a", "pt_b"}


@dataclass
class WorkerSpec:
    key: str
    label: str
    script: str
    channel_attr: str  # "ch0" or "ch1"


SPECS: List[WorkerSpec] = [
    WorkerSpec("pt_a", "PT-A gen", "pt_can_gen.py", "ch0"),
    WorkerSpec("pt_b", "PT-B gen", "pt_gen_b.py", "ch0"),
    WorkerSpec("uds", "UDS PID ECU", "uds_pid_ecu.py", "ch0"),
    WorkerSpec("dash", "Dash gen", "dash_gen.py", "ch1"),
    WorkerSpec("sniff", "Cluster sniff", "cluster_sniff.py", "ch1"),
]


@dataclass
class WorkerState:
    spec: WorkerSpec
    proc: Optional[subprocess.Popen] = None
    last_exit: Optional[int] = None

    @property
    def running(self) -> bool:
        return self.proc is not None and self.proc.poll() is None

    @property
    def pid(self) -> Optional[int]:
        if self.running and self.proc is not None:
            return self.proc.pid
        return None


@dataclass
class WorkerSupervisor:
    interface: str = "kvaser"
    ch0: str = "0"
    ch1: str = "1"
    bitrate: int = 500000
    workers: Dict[str, WorkerState] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not self.workers:
            self.workers = {s.key: WorkerState(s) for s in SPECS}

    def _channel(self, attr: str) -> str:
        return self.ch0 if attr == "ch0" else self.ch1

    def start(self, key: str) -> None:
        st = self.workers[key]
        if st.running:
            return
        script = TOOLS_DIR / st.spec.script
        args = [
            sys.executable,
            str(script),
            "--interface",
            self.interface,
            "--channel",
            self._channel(st.spec.channel_attr),
            "--bitrate",
            str(self.bitrate),
        ]
        st.proc = subprocess.Popen(
            args,
            cwd=str(ROOT),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        st.last_exit = None

    def stop(self, key: str) -> None:
        st = self.workers[key]
        if st.proc is None:
            return
        if st.proc.poll() is None:
            st.proc.terminate()
            try:
                st.proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                st.proc.kill()
                st.proc.wait(timeout=1.0)
        st.last_exit = st.proc.returncode
        st.proc = None

    def set_running(self, key: str, want: bool) -> None:
        if want:
            if key in PT_KEYS:
                for other in PT_KEYS:
                    if other != key:
                        self.stop(other)
            self.start(key)
        else:
            self.stop(key)

    def poll(self) -> None:
        for st in self.workers.values():
            if st.proc is not None and st.proc.poll() is not None:
                st.last_exit = st.proc.returncode
                st.proc = None

    def stop_all(self) -> None:
        for key in list(self.workers.keys()):
            self.stop(key)

    def status_line(self) -> str:
        parts = []
        for key, st in self.workers.items():
            if st.running:
                parts.append(f"{st.spec.label}=pid{st.pid}")
            elif st.last_exit is not None:
                parts.append(f"{st.spec.label}=exit{st.last_exit}")
            else:
                parts.append(f"{st.spec.label}=off")
        return "  ".join(parts)
