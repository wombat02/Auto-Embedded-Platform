# vcom_diag overview

Device-side ISO-TP + UDS on top of [`vcom`](../../vcom/README.md). The only file that includes `iso14229.h` is the diag wrapper; portable `vcom` stays free of UDS types.

## Chosen stack

| Role | Library | License |
| --- | --- | --- |
| CAN ISO-TP + UDS (firmware) | [driftregion/iso14229](https://github.com/driftregion/iso14229) **0.11.0** (embeds MIT `isotp-c`) | MIT |
| Host client / smoke | [python-udsoncan](https://github.com/pylessard/python-udsoncan) + [python-can](https://github.com/hardbyte/python-can) | MIT / LGPL host-only |

Pin the amalgamation under `third_party/iso14229`. Do not vendor a second `isotp-c`. Prefer MIT/BSD/Apache for anything linked into firmware; GPL host scanners stay out of process.

## Seams

- Send classic CAN via `vcom_tx_post_local`
- RX subscription on the response id → ISO-TP
- Millisecond clock from glue (`now_ms`)
- **`diag_link_poll` / `diag_server_poll` every 1 ms** (STmin); do not rely on a 10 ms `vcom_core_tick` alone

## Example profile

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

The STM32 example uses `diag_link` as a **client** on CAN0 (`aep_pid_client.c`). The host PID ECU owns physical ids `0x7E0` / `0x7E8` and answers RDBI `0xF201` / `0xF202`. See [DEMO_PT.md](../../examples/stm32_h5/DEMO_PT.md).

`diag_server` remains available in this library for product glue that needs an on-board UDS server.
