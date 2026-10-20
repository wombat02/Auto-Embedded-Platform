# STM32H523 example

CubeIDE project `stm32_h5` for `STM32H523VETX`. It is the **reference wiring** of the portable libraries to ThreadX, USBX, and STM32 HAL. Portable code stays at the repository root. Pins, IRQ handlers, RTOS threads, and demo CAN layouts stay in this folder (or in a CubeMX generate).

This example is an illustration and a local test bed. It is not a clone-and-build. Generate the Cube packs, paste the user-code blocks, then build. See [BRINGUP.md](BRINGUP.md).

Product firmware should copy the portable trees and implement the same port hooks. Keep vehicle-specific parsers in the product tree.

## Read in this order

| Document | What it answers |
| --- | --- |
| [BRINGUP.md](BRINGUP.md) | Generate from `.ioc`, paste list, include paths |
| [GLUE.md](GLUE.md) | Which `aep_*` file implements which portable contract |
| [INTEGRATION.md](INTEGRATION.md) | Board pins and include paths |
| [DEMO_PT.md](DEMO_PT.md) | Dual-bus PT / PID / cluster / dash demo and the host TUI |
| [docs/QUICKSTART.md](../../docs/QUICKSTART.md) | Host tests and smoke commands |
| [docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md) | Why each layer exists |
| [docs/LICENSING.md](../../docs/LICENSING.md) | MIT repo vs Cube packs |

## Glue file to portable module

| Portable module | Contract | Example glue |
| --- | --- | --- |
| [`aep_log`](../../aep_log/README.md) | [PORTING](../../aep_log/docs/PORTING.md) | `aep_port_log` in `Core/Src/aep_ucp_port.c` → `logi`/`logw`/`loge` |
| [`sig`](../../sig/README.md) | [PORTING](../../sig/docs/PORTING.md) · [catalog](../../sig/docs/CATALOG.md) | `Core/Src/aep_app.c` binds and snapshots the table |
| [`ucp`](../../ucp/README.md) | [PORTING](../../ucp/docs/PORTING.md) · [full guide](../../ucp/docs/UCP_PORTING_GUIDE.md) | `Core/Src/aep_ucp_port.c`, `Core/Src/aep_ucp_usb.c` |
| [`ucp_tlm`](../../ucp_tlm/README.md) | [PORTING](../../ucp_tlm/docs/PORTING.md) | `g_ucp_tlm_sig_app_vtable` registered in `aep_ucp_port.c` |
| [`ucp_cfg`](../../ucp_cfg/README.md) | [PORTING](../../ucp_cfg/docs/PORTING.md) | `g_ucp_cfg_app_vtable` + `ucp_cfg_bind` from `aep_app.c` |
| [`comp`](../../comp/README.md) | [PORTING](../../comp/docs/PORTING.md) | `aep_app.c` registry; `aep_pt_parser`, `aep_pid_client`, `aep_cluster_mod`, `aep_dash_mod` |
| [`store`](../../store/README.md) | [PORTING](../../store/docs/PORTING.md) | `Core/Src/aep_store_port.c` (RAM demo) |
| [`vcom`](../../vcom/README.md) | [PORTING](../../vcom/docs/PORTING.md) | `Core/Src/aep_fdcan_phy.c`, `Core/Src/aep_kline_phy.c` |
| [`vcom_diag`](../../vcom_diag/README.md) | [PORTING](../../vcom_diag/docs/PORTING.md) | `aep_pid_client.c` (`diag_link` client) |

Host-side counterparts live under [`tools/`](../../tools). The demo CAN ids in `Core/Inc/aep_demo_pt.h` match `tools/aep_demo_pt.py`.

Minimal UCP app sketch (unbuilt): [snippets/minimal_ucp_app.c](snippets/minimal_ucp_app.c).

## UCP apps registered here

`aep_ucp_port.c` installs three vtables:

| Index | App | Advertised |
| --- | --- | --- |
| 0 | `ucp_xfer` (`0x01` / V1.0) | No — routing and chunk pull/push only |
| 1 | `ucp_tlm` (`0x20` / V1.0) | Yes |
| 2 | `ucp_cfg` (`0x71` / V1.0) | Yes |

## Defaults

- `AEP_BRINGUP_MINIMAL=0` in `Core/Inc/aep_app.h` — `App_ThreadX_Init` calls `aep_app_init` (see [BRINGUP.md](BRINGUP.md)).
- Store port is RAM-backed for demonstration. A product implements `store_port_*` on real NVS.
- Board UDS path is a **client** (`aep_pid_client`); the host PID ECU owns `0x7E0` / `0x7E8`.
