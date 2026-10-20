# STM32H523 example integration

CubeIDE project `stm32_h5`, MCU `STM32H523VETX`. Hardware test bed for the portable libraries at the repository root.

This folder is **not** a standalone build. Generate HAL / ThreadX / USBX from `stm32_h5.ioc`, then follow [BRINGUP.md](BRINGUP.md). Project map: [README.md](README.md). Glue wiring: [GLUE.md](GLUE.md). Licenses: [docs/LICENSING.md](../../docs/LICENSING.md).

## Example board pins

`PA3` is driven as `USR_LED` (on some boards this net also controls a relay — confirm before flashing). `PE2` enables the external oscillator; `PH0` is HSE bypass in. `PD0` is the user button.

| Function | Pins |
| --- | --- |
| FDCAN1 | `PB8` RX, `PB9` TX, standby `PB4` (`CAN1_STB`) |
| FDCAN2 | `PB5` RX, `PB6` TX, standby `PB3` (`CAN2_STB`) |
| USART2 K-Line | `PD5` TX, `PD6` RX, `PD3` transceiver awake (`LIN_NSLP`) — owned by `aep_kline_phy.c`, not the `.ioc` |
| USB FS | Device connector (CDC ACM) |

Leave every other product pin unconfigured. Do not copy a full product `.ioc` into this tree.

## What this project may contain

| Tracked in git | From CubeMX generate |
| --- | --- |
| `aep_*`, `log.*`, `.ioc`, docs | HAL, CMSIS, ThreadX, USBX, startup, MX_* sources, linker script |

Include paths (Debug and Release) after generate:

```
../../../aep_log/include
../../../sig/include
../../../ucp/include
../../../ucp_tlm/include
../../../ucp_cfg/include
../../../comp/include
../../../store/include
../../../vcom/include
../../../vcom_diag/include
../../../vcom_diag/third_party/iso14229
```

Compile `iso14229.c` with `-DUDS_SYS=0 -DUDS_TP_ISOTP_C`. Host tests under each library's `test/` stay excluded from this build.

## After flash

- LED ~1 Hz blink (from `tx_app_thread_entry`)
- CDC ACM enumerates; open the port in a serial terminal for ASCII logs while UCP is not READY (baud is ignored on USB CDC). Use `python tools/ucp_host.py --port COMx` / the TUI for a live session (ASCII drain pauses while READY).
- Mock PT dual-bus demo (2CH Kvaser): see [DEMO_PT.md](DEMO_PT.md) and `tools/run_pt_demo.ps1`
  - CH0 ↔ CAN0: `pt_gen_a`/`pt_gen_b` + UDS PID ECU; board parser + PID client
  - CH1 ↔ CAN1: cluster `0x301`; optional dash `0x401`/`0x402` via `ucp_cfg.py`
  - Config: `python tools/ucp_cfg.py --port COMx …` (RAM NVS until reboot)
- Heartbeats `0x100` every 500 ms on both CAN0 (`…50`) and CAN1 (`…51`)
- Needs Kvaser **CANlib drivers** from kvaser.com (not pip). Vector XL is unrelated.
