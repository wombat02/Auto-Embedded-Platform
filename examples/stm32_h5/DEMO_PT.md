# Mock PT-CAN / PID / cluster / dash demo

Dual-bus bring-up with a **portable `comp` runtime**, **RX demux**, **NVM store**, and **UCP config app (0x71)**.

Project map: [README.md](README.md). Glue files: [GLUE.md](GLUE.md). Component model: [docs/FUTURE_COMPONENTS_CONFIG.md](../../docs/FUTURE_COMPONENTS_CONFIG.md). Tier A catalog: [sig/docs/CATALOG.md](../../sig/docs/CATALOG.md). Config app: [ucp_cfg](../../ucp_cfg/docs/OVERVIEW.md).

## Topology

| Kvaser | Board | Role |
| --- | --- | --- |
| Channel 0 | FDCAN1 / CAN0 | Host: `pt_gen_a.py` or `pt_gen_b.py` + `uds_pid_ecu.py`. Board: one PT parser profile + PID client |
| Channel 1 | FDCAN2 / CAN1 | Board: cluster TX `0x301` (default on); optional Tier B dash RX `0x401`/`0x402` |

Heartbeats `0x100` still run (`…50` on CAN0, `…51` on CAN1).

The board runs a UDS **client** (`aep_pid_client`). The host PID ECU owns `0x7E0` / `0x7E8`.

## Config-centric flow

```powershell
# After flash + CDC port:
python tools/ucp_cfg.py --port COMx list
python tools/ucp_cfg.py --port COMx slots
python tools/ucp_cfg.py --port COMx set cluster --enable 0
python tools/ucp_cfg.py --port COMx apply
python tools/ucp_cfg.py --port COMx save   # persist to flash NVS; reboot restores

# Switch PT profile A ↔ B (mutually exclusive layouts on CAN0):
python tools/ucp_cfg.py --port COMx set pt --profile b --enable 1
python tools/ucp_cfg.py --port COMx apply

# Enable Tier B dash on CAN1:
python tools/ucp_cfg.py --port COMx set dash --enable 1
python tools/ucp_cfg.py --port COMx apply
python tools/ucp_cfg.py --port COMx ads
```

Host manifest for dash names/scales: `tools/packs/demo_dash_v1.py` (`pack_id=0xD001`).

NVS is **RAM-backed** (`aep_store_port.c`) for demonstration. `ucp_cfg save` works until reboot. A product implements `store_port_*` on real media.

## Protocol (shared)

Constants: `Core/Inc/aep_demo_pt.h` ↔ `tools/aep_demo_pt.py`.

| ID | Bus | Profile | Content |
| --- | --- | --- | --- |
| `0x201`/`0x202` | CAN0 | PT-A | RPM BE, coolant; torque BE |
| `0x211`/`0x212` | CAN0 | PT-B | RPM/load/coolant LE; VSS/gear/throttle LE → Tier A |
| `0x301` | CAN1 | cluster | Remapped RPM/coolant/torque TX |
| `0x401`/`0x402` | CAN1 | dash | Steer deg; stalk flags → Tier B `0xF010+` |
| UDS | CAN0 | PID | Host answers RDBI `0xF201`/`0xF202` → ext `0xF001`/`0xF002` |

## Board sources

| File | Role |
| --- | --- |
| `portable/comp` | Runtime: descs, slots, apply/poll |
| `portable/vcom` + `vcom_rx_demux` | Wide bus subscribe → id handlers |
| `portable/store` + `aep_store_port.c` | NVS port (RAM demonstration) |
| `portable/ucp_cfg` | APP `0x71` list/set/apply/save/ads |
| `aep_pt_parser.*` | PT-A/B → Tier A |
| `aep_pid_client.*` | `diag_link` client → ext sigs |
| `aep_cluster_mod.*` | Sig → CAN1 `0x301` |
| `aep_dash_mod.*` | CAN1 dash → Tier B + pack ads |
| `aep_app.c` | Glue: materialize cfg, factory/NVM specs |

## Host tools

```powershell
pip install -r tools/requirements.txt
.\tools\run_pt_demo.ps1          # PT-A + PID ECU + cluster sniff
python tools/pt_gen_a.py --channel 0
python tools/pt_gen_b.py --channel 0
python tools/dash_gen.py --channel 1
python tools/ucp_host.py --port COMx
python tools/ucp_cfg.py --port COMx slots
```

### Demo TUI (recommended)

Single Textual app owns the CDC port (UCP TLM `0x20` + CFG `0x71` on one session), embeds the bar dashboard, and can start/stop CAN workers:

```powershell
pip install -r tools/requirements.txt
python tools/demo_tui.py --port COMx
```

- **Connect** — opens COM, negotiates link, loads slots into the config panel.
- **Workers** — PT-A / PT-B gen (exclusive), UDS PID ECU, **dash gen** (CAN1), cluster sniff (subprocesses; set Kvaser CH0/CH1 in the header). Match PT gen to board **PT profile**. Enable **Dash** in config + Apply before dash gen shows on the bars.
- **Center bars** — board **UCP USB** telemetry (APP `0x20`): PT + PID + Tier B dash (`0xF010+`) — **not** cluster sniff.
- **Config** — enable PT/PID/cluster/dash, PT profile A/B, **Apply** / **Save** (RAM NVS until reboot).
- Do **not** run `ucp_host.py` or `ucp_cfg.py` against the same COM while the TUI is connected.

**Cluster vs dash (board slots on CAN1):**

| Slot | Direction | What it does |
| --- | --- | --- |
| **Cluster** | Board **TX** `0x301` on CAN1 | Re-publishes RPM/coolant/torque from the sig table (as if driving an instrument cluster). Host `cluster_sniff` only **listens** on CH1 — it does not feed the TUI middle panel. |
| **Dash** | Board **RX** `0x401`/`0x402` on CAN1 | Tier B stalk/steer pack → ext signals `0xF010+` (enable + `dash_gen.py` on CH1). |

Shared modules: `tools/ucp_session.py`, `tools/ucp_cfg_client.py`, `tools/ucp_tlm_feed.py`, `tools/demo_workers.py`.
