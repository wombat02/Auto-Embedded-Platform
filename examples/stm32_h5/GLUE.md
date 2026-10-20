# Example glue map (`stm32_h5`)

How Cube/ThreadX/USBX on the STM32H523 example board connects to the portable libraries. Portable code never includes HAL or IRQ names; all of that stays here (or in a CubeMX generate).

Project map: [README.md](README.md). Generate and paste: [BRINGUP.md](BRINGUP.md). Board pins: [INTEGRATION.md](INTEGRATION.md). Demo procedure: [DEMO_PT.md](DEMO_PT.md).

## Layers

```text
main / clocks / GPIO / USB PCD / FDCAN MX   (Cube generate + USER CODE paste)
        |
   ThreadX + USBX (CDC ACM)                 (Cube generate)
        |
   aep_* glue (this folder's Core — tracked)
        |
   aep_log | sig | ucp | ucp_tlm | ucp_cfg | comp | store | vcom | vcom_diag
        |
   tools/ucp_host.py , tools/ucp_cfg.py , tools/pt_gen_*.py , tools/demo_tui.py
```

## File ownership

| File | Role | Portable contract |
| --- | --- | --- |
| `Core/Src/aep_app.c` | Orchestration: demux, `comp` apply, store load, sig bind, threads | [comp](../../comp/docs/PORTING.md), [sig](../../sig/docs/PORTING.md) |
| `Core/Inc/aep_comp.h` | Shim → portable `comp.h` | [comp](../../comp/README.md) |
| `Core/Inc/aep_demo_pt.h` | PT-A/B, dash, UDS client ids, pack ids (↔ `tools/aep_demo_pt.py`) | [DEMO_PT.md](DEMO_PT.md) |
| `Core/Src/aep_pt_parser.c` | PT-A/B CAN0 → Tier A (via demux) | [comp](../../comp/docs/PORTING.md) |
| `Core/Src/aep_pid_client.c` | Board UDS **client** → extension signals | [vcom_diag](../../vcom_diag/docs/PORTING.md) |
| `Core/Src/aep_cluster_mod.c` | Sig → CAN1 `0x301` | [comp](../../comp/docs/PORTING.md) |
| `Core/Src/aep_dash_mod.c` | CAN1 dash → Tier B + pack ads | [sig catalog](../../sig/docs/CATALOG.md) |
| `Core/Src/aep_store_port.c` | RAM `store_port_*` (demo only) | [store](../../store/docs/PORTING.md) |
| `Core/Src/aep_ucp_port.c` | Strong `ucp_port_*` + `aep_port_log`. Apps: xfer, tlm, cfg | [ucp](../../ucp/docs/PORTING.md), [aep_log](../../aep_log/docs/PORTING.md), [ucp_tlm](../../ucp_tlm/docs/PORTING.md), [ucp_cfg](../../ucp_cfg/docs/PORTING.md) |
| `Core/Src/log.c` | USB log ring (`logi`/`logw`/`loge`) — sink for `aep_port_log` | [aep_log](../../aep_log/README.md) |
| `Core/Src/aep_fdcan_phy.c` | FDCAN HAL → `vcom_can_phy_t` | [vcom](../../vcom/docs/PORTING.md) |
| `Core/Src/aep_kline_phy.c` | USART2 + `LIN_NSLP` → `vcom_kline_phy_t` | [vcom](../../vcom/docs/PORTING.md) |
| `Core/Src/aep_ucp_usb.c` | CDC ↔ `ucp_session`: forever OUT RX; poll/TX on USBX app thread | [ucp](../../ucp/docs/PORTING.md) |
| `snippets/minimal_ucp_app.c` | Unbuilt REQ_RSP sketch | [ucp app layer](../../ucp/docs/UCP_APPLICATION_LAYER.md) |

## Runtime sequence

1. Com thread: PHYs → buses @ 500 kbit/s → demux attach → heartbeats.
2. `store_init` → load slot specs (or factory) → `ucp_cfg_bind` → `comp_runtime_apply`. Store is RAM; commit is lost on reboot.
3. Loop: USB poll on USBX thread → `drain_rx` (demux inside subscribe) → `comp_runtime_poll` → `vcom_core_tick` → tlm submit.
4. Host: `ucp_cfg.py` for slots; `pt_gen_a`/`pt_gen_b`/`dash_gen` on CAN — see [DEMO_PT.md](DEMO_PT.md).

## Bring-up gate

`AEP_BRINGUP_MINIMAL` in `Core/Inc/aep_app.h`: used only in the Cube paste for `App_ThreadX_Init`. `0` = call `aep_app_init` (default).
