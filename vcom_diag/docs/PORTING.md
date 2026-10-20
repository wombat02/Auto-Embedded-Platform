# vcom_diag porting

## Compile

Build `iso14229.c` with:

```
-DUDS_SYS=0 -DUDS_TP_ISOTP_C
```

Include `vcom_diag/include` and `vcom_diag/third_party/iso14229`.

## Glue responsibilities

1. Provide a started `vcom_core_t` bus for diagnostics.
2. Call `diag_server_init` / `diag_link_init` with request/response ids and `now_ms`.
3. Poll every **1 ms** (`diag_server_poll` or `diag_link_poll`).
4. Keep `vcom_core_tick` on its own cadence for TX scheduling.

## API surface

- `diag_link_init` / `diag_link_subscribe_rx`
- `diag_link_poll`
- `diag_link_session_default`, `diag_link_tester_present`, `diag_link_read_did`
- Server side: `diag_server_*`

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

`examples/stm32_h5/Core/Src/aep_pid_client.c` is the `diag_link` client. ISO-TP is polled from a 1 ms ThreadX timer in `aep_app.c`.
